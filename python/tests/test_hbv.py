"""HBV-96 model tests — instantiation, options, water balance and behaviour.

The HBV-96 structure (Lindström et al., 1997) is integrated by the ODE solver
(as Socont), so these tests verify the structure wiring (snow routine with
liquid water retention and refreezing, beta-function soil moisture routine,
non-linear upper zone, linear lower zone and MAXBAS routing) and the water
balance closure rather than a discrete reference implementation.
"""

from __future__ import annotations

from datetime import date, timedelta
from pathlib import Path

import numpy as np
import pytest

import hydrobricks as hb
import hydrobricks.models as models

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_START = date(1981, 1, 1)
_N_2Y = 730  # 1981 + 1982 — neither is a leap year

_DEFAULT_PARAMS = {
    "cfmax": 3.0,
    "tt": 0.0,
    "fc": 200.0,
    "lp": 0.9,
    "beta": 2.0,
    "k_uz": 0.1,
    "alpha": 1.0,
    "perc": 0.5,
    "k_lz": 0.01,
    "maxbas": 1.0,
}


def _hu_csv(tmp_path: Path) -> Path:
    """Write a single-HU CSV (elevation 1000 m, area 1 km²)."""
    p = tmp_path / "hydro_units.csv"
    p.write_text("id,elevation,area\n-,m,m^2\n1,1000,1000000\n")
    return p


def _meteo_csv_series(tmp_path: Path, precip, pet, temp) -> Path:
    """Write a meteo CSV from per-day precipitation, PET and temperature series."""
    lines = ["date,precip(mm/day),pet(mm/day),temp(C)"]
    for i, (p, e, t) in enumerate(zip(precip, pet, temp)):
        d = _START + timedelta(days=i)
        lines.append(f"{d.strftime('%d/%m/%Y')},{p:.6f},{e:.6f},{t:.6f}")
    path = tmp_path / "meteo.csv"
    path.write_text("\n".join(lines) + "\n")
    return path


def _meteo_csv_seasonal(tmp_path: Path, n_days: int, P: float, PET: float) -> Path:
    """Write a meteo CSV with constant P and PET and a seasonal temperature cycle
    (snow accumulation in winter, melt in summer)."""
    days = np.arange(n_days)
    temp = 5.0 - 12.0 * np.cos(2.0 * np.pi * days / 365.25)
    return _meteo_csv_series(tmp_path, [P] * n_days, [PET] * n_days, temp)


def _load_forcing(hydro_units, meteo_path: Path) -> hb.Forcing:
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        meteo_path,
        column_time="date",
        time_format="%d/%m/%Y",
        content={
            "precipitation": "precip(mm/day)",
            "pet": "pet(mm/day)",
            "temperature": "temp(C)",
        },
    )
    # ref_elevation=1000 matches the single HU elevation; gradient=0 → no correction
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=1000, gradient=0.0
    )
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=1000, gradient=0.0
    )
    forcing.spatialize_from_station_data(variable="pet")
    return forcing


def _run_model(tmp_path, meteo_path, n_days, params=None, **model_options) -> tuple:
    """Build and run an HBV-96 model on a given meteo file; return (model, forcing)."""
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        _hu_csv(tmp_path), column_elevation="elevation", column_area="area"
    )
    forcing = _load_forcing(hydro_units, meteo_path)

    model = models.HBV96(**model_options)
    parameters = model.generate_parameters()
    values = dict(_DEFAULT_PARAMS)
    if params:
        values.update(params)
    parameters.set_values(values)

    end_date = (_START + timedelta(days=n_days - 1)).strftime("%Y-%m-%d")
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(tmp_path),
        start_date=_START.strftime("%Y-%m-%d"),
        end_date=end_date,
    )
    model.run(parameters=parameters, forcing=forcing)
    return model, forcing


def _run(
    tmp_path: Path,
    *,
    P=5.0,
    PET=1.5,
    n_days=_N_2Y,
    params=None,
    **model_options,
) -> tuple:
    """Run HBV-96 with seasonal temperature forcing; return (model, forcing)."""
    meteo = _meteo_csv_seasonal(tmp_path, n_days, P, PET)
    return _run_model(tmp_path, meteo, n_days, params=params, **model_options)


def _balance(model, forcing) -> float:
    precip = forcing.get_total_precipitation()
    discharge = model.get_total_outlet_discharge()
    et = model.get_total_et()
    storage_change = model.get_total_water_storage_changes()
    snow_change = model.get_total_snow_storage_changes()
    return discharge + et + storage_change + snow_change - precip


# ---------------------------------------------------------------------------
# A — Instantiation and options
# ---------------------------------------------------------------------------


def test_hbv96_instantiation():
    assert models.HBV96().name == "hbv96"


def test_hbv_base_class_is_abstract():
    with pytest.raises(TypeError):
        models.HBV()


def test_hbv96_generate_parameters_contains_literature_names():
    parameters = models.HBV96().generate_parameters()
    for name in (
        "tt",
        "cfmax",
        "cfr",
        "cwh",
        "fc",
        "lp",
        "beta",
        "cflux",
        "k_uz",
        "alpha",
        "alfa",  # SMHI spelling
        "perc",
        "k_lz",
        "k4",  # SMHI lower zone recession coefficient
        "maxbas",
        "cevpf",  # ET correction factor
    ):
        assert parameters.has(name), f"parameter {name!r} not found"


def test_hbv96_parameter_lookup_is_case_insensitive():
    parameters = models.HBV96().generate_parameters()
    parameters.set_values({"FC": 200.0, "PERC": 0.5, "ALFA": 1.0})
    assert parameters.get("fc") == 200.0
    assert parameters.get("perc") == 0.5
    assert parameters.get("upper_zone:alpha") == 1.0


def test_hbv96_without_snow_water_retention():
    model = models.HBV96(
        snow_water_retention_process=None,
        snow_refreezing_process=None,
        rain_to_snowpack=False,
    )
    parameters = model.generate_parameters()
    assert not parameters.has("cwh")
    assert not parameters.has("cfr")


def test_hbv96_refreezing_requires_water_retention():
    with pytest.raises(hb.ConfigurationError):
        models.HBV96(snow_water_retention_process=None, rain_to_snowpack=False)


def test_hbv96_rain_to_snowpack_is_default():
    assert models.HBV96().options["rain_to_snowpack"] is True


def test_hbv96_rain_to_snowpack_requires_water_retention():
    with pytest.raises(hb.ConfigurationError):
        models.HBV96(snow_water_retention_process=None, snow_refreezing_process=None)


def test_hbv96_refreezing_requires_degree_day_melt():
    with pytest.raises(hb.ConfigurationError):
        models.HBV96(snow_melt_process="melt:temperature_index")


# ---------------------------------------------------------------------------
# B — Water balance
# ---------------------------------------------------------------------------


def test_hbv96_water_balance_closes(tmp_path):
    model, forcing = _run(tmp_path, record_all=True)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_hbv96_water_balance_closes_with_maxbas(tmp_path):
    model, forcing = _run(tmp_path, params={"maxbas": 4.0}, record_all=True)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_hbv96_water_balance_closes_with_capillary_flux(tmp_path):
    model, forcing = _run(tmp_path, params={"cflux": 1.0}, record_all=True)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_hbv96_water_balance_closes_without_snow_retention(tmp_path):
    model, forcing = _run(
        tmp_path,
        record_all=True,
        snow_water_retention_process=None,
        snow_refreezing_process=None,
        rain_to_snowpack=False,
    )
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_hbv96_water_balance_closes_without_rain_to_snowpack(tmp_path):
    model, forcing = _run(tmp_path, record_all=True, rain_to_snowpack=False)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


# ---------------------------------------------------------------------------
# C — Behaviour and limit conditions
# ---------------------------------------------------------------------------


def test_hbv96_discharge_non_negative_no_nan(tmp_path):
    model, _ = _run(tmp_path)
    q = model.get_outlet_discharge()
    assert np.all(q >= 0.0)
    assert not np.any(np.isnan(q))


def test_hbv96_zero_precipitation_produces_no_discharge(tmp_path):
    model, _ = _run(tmp_path, P=0.0)
    q = model.get_outlet_discharge()
    assert np.sum(q) == pytest.approx(0, abs=1e-8)


def test_hbv96_maxbas_spreads_the_peak(tmp_path):
    """A larger MAXBAS must lower and spread a storm peak (same total volume)."""

    def _q_storm(maxbas: float) -> np.ndarray:
        # Warm (rain only), dry except a 3-day 50 mm/d storm.
        n_days = 60
        precip = [0.0] * n_days
        precip[10:13] = [50.0, 50.0, 50.0]
        meteo = _meteo_csv_series(tmp_path, precip, [1.0] * n_days, [10.0] * n_days)
        model, _ = _run_model(
            tmp_path, meteo, n_days, params={"maxbas": maxbas, "k_uz": 0.3}
        )
        return model.get_outlet_discharge()

    q1 = _q_storm(1.0)
    q4 = _q_storm(4.0)
    assert np.max(q4) < np.max(q1)
    assert np.sum(q4) == pytest.approx(np.sum(q1), rel=1e-3)


def test_hbv96_rain_on_cold_snowpack_is_retained(tmp_path):
    """Rain falling on a cold snowpack must be retained in the liquid water
    storage (and refreeze) instead of reaching the ground directly."""

    def _total_q(rain_to_snowpack: bool) -> float:
        # 25 cold days build the snowpack, then 5 days at 1 °C: half of the
        # precipitation falls as rain on a snowpack that does not melt (tt=2).
        # The holding capacity (cwh × SWE = 25 mm) exceeds the rain (15 mm).
        n_days = 30
        temp = [-10.0] * 25 + [1.0] * 5
        meteo = _meteo_csv_series(tmp_path, [10.0] * n_days, [1.0] * n_days, temp)
        model, forcing = _run_model(
            tmp_path,
            meteo,
            n_days,
            params={"tt": 2.0, "fc": 30.0},
            rain_to_snowpack=rain_to_snowpack,
            record_all=True,
        )
        assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)
        return float(np.sum(model.get_outlet_discharge()))

    assert _total_q(True) == pytest.approx(0, abs=1e-6)
    assert _total_q(False) > 0.01


def test_hbv96_alpha_zero_behaves_linearly(tmp_path):
    """With alpha=0 the upper zone is a linear reservoir; the run must succeed."""
    model, forcing = _run(tmp_path, params={"alpha": 0.0}, record_all=True)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


# ---------------------------------------------------------------------------
# D — Precipitation correction factors (RFCF / SFCF)
# ---------------------------------------------------------------------------


def test_hbv96_generate_parameters_contains_correction_factors():
    parameters = models.HBV96().generate_parameters()
    for name in ("rfcf", "rcf", "sfcf", "scf"):
        assert parameters.has(name), f"parameter {name!r} not found"


def _subdir(tmp_path: Path, name: str) -> Path:
    """Create and return a fresh subdirectory (each model run needs its own)."""
    d = tmp_path / name
    d.mkdir()
    return d


def test_hbv96_snowfall_correction_increases_discharge(tmp_path):
    """SFCF > 1 corrects snow gauge undercatch: more snow accumulates, later melts
    and raises the total discharge (the default 1.0 leaves precipitation unchanged)."""
    base, _ = _run(_subdir(tmp_path, "base"))
    corrected, _ = _run(_subdir(tmp_path, "sfcf"), params={"sfcf": 1.5})
    assert corrected.get_total_outlet_discharge() > base.get_total_outlet_discharge()


def test_hbv96_rainfall_correction_increases_discharge(tmp_path):
    """RFCF > 1 corrects rain gauge undercatch and raises the total discharge."""
    base, _ = _run(_subdir(tmp_path, "base"))
    corrected, _ = _run(_subdir(tmp_path, "rfcf"), params={"rfcf": 1.3})
    assert corrected.get_total_outlet_discharge() > base.get_total_outlet_discharge()


def test_hbv96_unit_correction_factors_are_neutral(tmp_path):
    """Explicit factors of 1.0 reproduce the default (no correction) run exactly."""
    default, _ = _run(_subdir(tmp_path, "default"))
    explicit, _ = _run(_subdir(tmp_path, "explicit"), params={"rfcf": 1.0, "sfcf": 1.0})
    assert explicit.get_total_outlet_discharge() == pytest.approx(
        default.get_total_outlet_discharge(), rel=1e-9
    )


# ---------------------------------------------------------------------------
# D2 — ET correction factor (cevpf)
# ---------------------------------------------------------------------------


def test_hbv96_et_correction_factor_increases_et(tmp_path):
    """A larger ET correction factor (cevpf > 1) raises evapotranspiration and lowers
    total discharge; the water balance still closes."""
    base, _ = _run(_subdir(tmp_path, "base"), record_all=True)
    corrected, forcing = _run(
        _subdir(tmp_path, "cevpf"), params={"cevpf": 1.5}, record_all=True
    )
    assert corrected.get_total_et() > base.get_total_et()
    assert corrected.get_total_outlet_discharge() < base.get_total_outlet_discharge()
    assert _balance(corrected, forcing) == pytest.approx(0, abs=1e-6)


def test_hbv96_unit_et_correction_factor_is_neutral(tmp_path):
    """An explicit cevpf of 1.0 reproduces the default (no correction) run."""
    default, _ = _run(_subdir(tmp_path, "default"))
    explicit, _ = _run(_subdir(tmp_path, "explicit"), params={"cevpf": 1.0})
    assert explicit.get_total_outlet_discharge() == pytest.approx(
        default.get_total_outlet_discharge(), rel=1e-9
    )


def test_hbv96_per_cover_et_correction_alias():
    """With per-class soils, the ET correction factor is exposed per cover
    (cevpf_<cover>), so e.g. forests can evaporate more than open areas."""
    parameters = models.HBV96(
        land_cover_names=["open", "forest"], land_cover_types=["open", "forest"]
    ).generate_parameters()
    for name in ("cevpf_open", "cevpf_forest"):
        assert parameters.has(name), f"parameter {name!r} not found"


# ---------------------------------------------------------------------------
# E — Multiple land covers and per-class soil moisture
# ---------------------------------------------------------------------------

_COVERS = (["open", "forest"], ["ground", "ground"])

_PARAMS_2COVER_SHARED = {
    "cfmax": 3.0,
    "tt": 0.0,
    "k_uz": 0.1,
    "alpha": 1.0,
    "perc": 0.5,
    "k_lz": 0.01,
    "maxbas": 1.0,
    "fc": 200.0,
    "lp": 0.9,
    "beta_open": 2.0,
    "beta_forest": 3.0,
}

_PARAMS_2COVER_PERCLASS = {
    "cfmax": 3.0,
    "tt": 0.0,
    "k_uz": 0.1,
    "alpha": 1.0,
    "perc": 0.5,
    "k_lz": 0.01,
    "maxbas": 1.0,
    "fc_open": 200.0,
    "fc_forest": 250.0,
    "lp_open": 0.9,
    "lp_forest": 0.8,
    "beta_open": 2.0,
    "beta_forest": 3.0,
}


def _run_2cover(tmp_path, *, share_soil, params, P=5.0, PET=1.5, n_days=_N_2Y) -> tuple:
    """Build and run an HBV-96 model with two 'ground' land covers (open, forest)."""
    hydro_units = hb.HydroUnits(
        land_cover_types=_COVERS[1], land_cover_names=_COVERS[0]
    )
    hu_csv = tmp_path / "hydro_units.csv"
    hu_csv.write_text(
        "id,elevation,area_open,area_forest\n" "-,m,m^2,m^2\n" "1,1000,600000,400000\n"
    )
    hydro_units.load_from_csv(
        hu_csv,
        column_elevation="elevation",
        columns_areas={"open": "area_open", "forest": "area_forest"},
    )
    forcing = _load_forcing(hydro_units, _meteo_csv_seasonal(tmp_path, n_days, P, PET))

    model = models.HBV96(
        land_cover_names=_COVERS[0],
        land_cover_types=_COVERS[1],
        share_soil=share_soil,
        record_all=True,
    )
    parameters = model.generate_parameters()
    parameters.set_values(params)

    end_date = (_START + timedelta(days=n_days - 1)).strftime("%Y-%m-%d")
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(tmp_path),
        start_date=_START.strftime("%Y-%m-%d"),
        end_date=end_date,
    )
    model.run(parameters=parameters, forcing=forcing)
    return model, forcing


def test_hbv96_per_class_soil_exposes_per_cover_aliases():
    """With several covers and per-class soils, the soil/recharge aliases become
    cover-specific (fc_<cover>, lp_<cover>, beta_<cover>); the bare names are gone."""
    parameters = models.HBV96(
        land_cover_names=_COVERS[0], land_cover_types=_COVERS[1]
    ).generate_parameters()
    for name in (
        "fc_open",
        "fc_forest",
        "lp_open",
        "lp_forest",
        "beta_open",
        "beta_forest",
    ):
        assert parameters.has(name), f"parameter {name!r} not found"
    for name in ("fc", "lp", "beta"):
        assert not parameters.has(name), f"unexpected bare alias {name!r}"


def test_hbv96_shared_soil_keeps_bare_soil_aliases():
    """With share_soil the single store keeps fc/lp; beta stays per-cover (it lives
    on each land cover's infiltration process)."""
    parameters = models.HBV96(
        land_cover_names=_COVERS[0], land_cover_types=_COVERS[1], share_soil=True
    ).generate_parameters()
    for name in ("fc", "lp", "beta_open", "beta_forest"):
        assert parameters.has(name), f"parameter {name!r} not found"
    assert not parameters.has("beta")


def test_hbv96_per_class_soil_water_balance_closes(tmp_path):
    model, forcing = _run_2cover(
        tmp_path, share_soil=False, params=_PARAMS_2COVER_PERCLASS
    )
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_hbv96_shared_soil_water_balance_closes(tmp_path):
    model, forcing = _run_2cover(
        tmp_path, share_soil=True, params=_PARAMS_2COVER_SHARED
    )
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_hbv96_per_class_soil_capillary_fanout_balance_closes(tmp_path):
    """An active capillary flux (cflux > 0) fans out to each per-class soil
    (area-weighted) and the water balance still closes."""
    params = dict(_PARAMS_2COVER_PERCLASS)
    params["cflux"] = 1.0
    model, forcing = _run_2cover(tmp_path, share_soil=False, params=params)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def _run_open_forest(
    tmp_path,
    *,
    forest_frac,
    params,
    ic=None,
    interception=True,
    P=5.0,
    PET=2.0,
    n_days=_N_2Y,
) -> tuple:
    """Run HBV-96 with an open ('ground') and a forest cover (the latter optionally
    intercepting rain in a canopy)."""
    open_frac = 1.0 - forest_frac
    hydro_units = hb.HydroUnits(
        land_cover_types=["ground", "forest"], land_cover_names=["open", "forest"]
    )
    hu_csv = tmp_path / "hydro_units.csv"
    area = 1_000_000
    hu_csv.write_text(
        "id,elevation,area_open,area_forest\n"
        "-,m,m^2,m^2\n"
        f"1,1000,{open_frac * area:.0f},{forest_frac * area:.0f}\n"
    )
    hydro_units.load_from_csv(
        hu_csv,
        column_elevation="elevation",
        columns_areas={"open": "area_open", "forest": "area_forest"},
    )
    forcing = _load_forcing(hydro_units, _meteo_csv_seasonal(tmp_path, n_days, P, PET))

    model = models.HBV96(
        land_cover_names=["open", "forest"],
        land_cover_types=["ground", "forest"],
        forest_interception=interception,
        record_all=True,
    )
    parameters = model.generate_parameters()
    values = dict(params)
    if ic is not None:
        values["ic"] = ic
    parameters.set_values(values)

    end_date = (_START + timedelta(days=n_days - 1)).strftime("%Y-%m-%d")
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(tmp_path),
        start_date=_START.strftime("%Y-%m-%d"),
        end_date=end_date,
    )
    model.run(parameters=parameters, forcing=forcing)
    return model, forcing


_PARAMS_OPEN_FOREST = {
    "cfmax": 3.0,
    "tt": 0.0,
    "k_uz": 0.1,
    "alpha": 1.0,
    "perc": 0.5,
    "k_lz": 0.01,
    "maxbas": 1.0,
    "fc_open": 200.0,
    "fc_forest": 200.0,
    "lp_open": 0.9,
    "lp_forest": 0.9,
    "beta_open": 2.0,
    "beta_forest": 2.0,
}


def test_hbv96_forest_interception_is_off_by_default():
    """Forest interception is opt-in: by default a forest cover exposes no canopy
    interception capacity parameter (no canopy is generated)."""
    assert models.HBV96().options["forest_interception"] is False
    parameters = models.HBV96(
        land_cover_names=["open", "forest"], land_cover_types=["open", "forest"]
    ).generate_parameters()
    assert not parameters.has("ic")


def test_hbv96_forest_exposes_interception_capacity_alias():
    parameters = models.HBV96(
        land_cover_names=["open", "forest"],
        land_cover_types=["open", "forest"],
        forest_interception=True,
    ).generate_parameters()
    assert parameters.has("ic"), "interception capacity alias 'ic' not found"


def test_hbv96_forest_interception_water_balance_closes(tmp_path):
    """The canopy (a per-cover surface component) intercepts rain, evaporates at PET
    and passes the rest as throughfall; the catchment balance must close (forest
    fraction < 1 exercises the area-weighting of the canopy storage and ET)."""
    model, forcing = _run_open_forest(
        _subdir(tmp_path, "fb"), forest_frac=0.4, params=_PARAMS_OPEN_FOREST, ic=3.0
    )
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_hbv96_forest_interception_reduces_discharge(tmp_path):
    """A larger interception capacity evaporates more rain, lowering total discharge."""
    low, _ = _run_open_forest(
        _subdir(tmp_path, "low"), forest_frac=0.4, params=_PARAMS_OPEN_FOREST, ic=0.0
    )
    high, _ = _run_open_forest(
        _subdir(tmp_path, "high"), forest_frac=0.4, params=_PARAMS_OPEN_FOREST, ic=5.0
    )
    assert high.get_total_outlet_discharge() < low.get_total_outlet_discharge()


def test_hbv96_forest_without_interception_runs_and_balances(tmp_path):
    """With interception disabled (the default), a forest cover behaves like a generic
    soil cover: the model runs and the water balance closes."""
    model, forcing = _run_open_forest(
        _subdir(tmp_path, "noint"),
        forest_frac=0.4,
        params=_PARAMS_OPEN_FOREST,
        interception=False,
    )
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_hbv96_single_cover_keeps_legacy_aliases():
    """A single land cover (the default) keeps the bare fc/lp/beta aliases and the
    'soil_moisture' store name, unchanged from before per-class soils."""
    model = models.HBV96()
    assert model._shared_soil is True
    assert "soil_moisture" in model.structure
    parameters = model.generate_parameters()
    for name in ("fc", "lp", "beta"):
        assert parameters.has(name)


def test_hbv96_default_cover_is_open():
    """HBV defaults to an 'open' land cover (the HBV 'open areas' class)."""
    model = models.HBV96()
    assert model.land_cover_names == ["open"]
    assert model.land_cover_types == ["open"]


def test_hbv96_open_cover_type_is_generic_alias():
    """The 'open' land cover type is an alias of the generic (ground) cover: it is a
    soil-bearing cover with the usual soil parameters."""
    model = models.HBV96(land_cover_names=["open"], land_cover_types=["open"])
    assert model._shared_soil is True
    assert "soil_moisture" in model.structure
    parameters = model.generate_parameters()
    for name in ("fc", "lp", "beta"):
        assert parameters.has(name)


def test_hbv96_ground_cover_still_accepted():
    """'ground' is kept as a backward-compatible alias: an explicit 'ground' cover is
    accepted (HBV's allowed list no longer lists it) and builds a generic soil cover."""
    model = models.HBV96(land_cover_names=["ground"], land_cover_types=["ground"])
    assert "ground" not in model.allowed_land_cover_types  # dropped from the list
    assert "soil_moisture" in model.structure
    parameters = model.generate_parameters()
    for name in ("fc", "lp", "beta"):
        assert parameters.has(name)


def test_hbv96_open_cover_water_balance_closes(tmp_path):
    """An 'open' single-cover catchment runs and closes the water balance, exactly like
    the default 'ground' cover."""
    hydro_units = hb.HydroUnits(land_cover_types=["open"], land_cover_names=["open"])
    hu_csv = tmp_path / "hydro_units.csv"
    hu_csv.write_text("id,elevation,area_open\n-,m,m^2\n1,1000,1000000\n")
    hydro_units.load_from_csv(
        hu_csv, column_elevation="elevation", columns_areas={"open": "area_open"}
    )
    forcing = _load_forcing(hydro_units, _meteo_csv_seasonal(tmp_path, _N_2Y, 5.0, 1.5))

    model = models.HBV96(
        land_cover_names=["open"], land_cover_types=["open"], record_all=True
    )
    parameters = model.generate_parameters()
    parameters.set_values(_DEFAULT_PARAMS)
    end_date = (_START + timedelta(days=_N_2Y - 1)).strftime("%Y-%m-%d")
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(tmp_path),
        start_date=_START.strftime("%Y-%m-%d"),
        end_date=end_date,
    )
    model.run(parameters=parameters, forcing=forcing)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


# ---------------------------------------------------------------------------
# F — Lake (exclusive open-water cover)
# ---------------------------------------------------------------------------

_PARAMS_GROUND_LAKE = {
    "cfmax": 3.0,
    "tt": 0.0,
    "fc": 200.0,
    "lp": 0.9,
    "beta": 2.0,
    "k_uz": 0.1,
    "alpha": 1.0,
    "perc": 0.5,
    "k_lz": 0.01,
    "maxbas": 1.0,
    "k_lake": 0.2,
}


def _run_ground_lake(tmp_path, *, P=5.0, PET=1.5, n_days=_N_2Y, params=None) -> tuple:
    """Run HBV-96 on a 2-unit catchment: one all-ground unit and one all-lake unit
    (lakes are exclusive). Returns (model, forcing, results)."""
    hydro_units = hb.HydroUnits(
        land_cover_types=["ground", "lake"], land_cover_names=["ground", "lake"]
    )
    hu_csv = tmp_path / "hydro_units.csv"
    hu_csv.write_text(
        "id,elevation,area_ground,area_lake\n"
        "-,m,m^2,m^2\n"
        "1,1000,1000000,0\n"
        "2,1000,0,500000\n"
    )
    hydro_units.load_from_csv(
        hu_csv,
        column_elevation="elevation",
        columns_areas={"ground": "area_ground", "lake": "area_lake"},
    )
    forcing = _load_forcing(hydro_units, _meteo_csv_seasonal(tmp_path, n_days, P, PET))

    model = models.HBV96(
        land_cover_names=["ground", "lake"],
        land_cover_types=["ground", "lake"],
        record_all=True,
    )
    parameters = model.generate_parameters()
    parameters.set_values(params or _PARAMS_GROUND_LAKE)

    end_date = (_START + timedelta(days=n_days - 1)).strftime("%Y-%m-%d")
    out = tmp_path / "out"
    out.mkdir()
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(out),
        start_date=_START.strftime("%Y-%m-%d"),
        end_date=end_date,
    )
    model.run(parameters=parameters, forcing=forcing)
    model.dump_outputs(str(out))
    results = hb.Results(str(out / "results.nc"))
    return model, forcing, results


def test_hbv96_lake_exposes_outflow_alias():
    parameters = models.HBV96(
        land_cover_names=["ground", "lake"], land_cover_types=["ground", "lake"]
    ).generate_parameters()
    assert parameters.has("k_lake"), "lake outflow alias 'k_lake' not found"


def test_hbv96_lake_water_balance_closes(tmp_path):
    """A lake unit takes all precipitation directly into open water, evaporates at PET
    and drains through a linear outflow; the catchment balance must close."""
    model, forcing, _ = _run_ground_lake(tmp_path)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_hbv96_lake_unit_carries_no_snowpack(tmp_path):
    """The lake structure variant has no snowpack: the lake unit's snowpack state is
    omitted (NaN), while the ground unit carries one."""
    _, _, results = _run_ground_lake(tmp_path, PET=0.5)
    structure_ids = results.get_hydro_units_structure_ids()
    # Two variants are in use (base/ground and lake).
    assert len(set(structure_ids.tolist())) == 2
    swe = results.get_hydro_units_values("ground_snowpack:snow_content")
    # The ground unit (index 0) has snowpack state; the lake unit (index 1) does not.
    assert not bool(np.all(np.isnan(swe[0, :])))
    assert bool(np.all(np.isnan(swe[1, :])))


def test_hbv96_lake_evaporates_at_potential_rate(tmp_path):
    """Open-water evaporation removes water at the potential rate: with PET above the
    precipitation, a lake unit's contribution should be ET-limited (lower discharge
    than with no evaporation)."""
    wet, forcing_wet, _ = _run_ground_lake(_subdir(tmp_path, "a"), P=5.0, PET=0.0)
    dry, forcing_dry, _ = _run_ground_lake(_subdir(tmp_path, "b"), P=5.0, PET=4.0)
    assert dry.get_total_outlet_discharge() < wet.get_total_outlet_discharge()
    assert _balance(dry, forcing_dry) == pytest.approx(0, abs=1e-6)


# ---------------------------------------------------------------------------
# G — Glacier (Socont-style)
# ---------------------------------------------------------------------------

_PARAMS_GROUND_GLACIER = {
    "cfmax": 3.0,
    "a_ice": 6.0,
    "tt": 0.0,
    "fc": 200.0,
    "lp": 0.9,
    "beta": 2.0,
    "k_uz": 0.1,
    "alpha": 1.0,
    "perc": 0.5,
    "k_lz": 0.01,
    "maxbas": 1.0,
    "k_snow": 0.2,
    "k_ice": 0.3,
}


def _run_ground_glacier(
    tmp_path, *, P=5.0, PET=1.5, n_days=_N_2Y, params=None, infinite_storage=False
) -> tuple:
    """Run HBV-96 on a 2-unit catchment: an all-ground unit and a ground+glacier unit.
    Returns (model, forcing, results). The glacier ice storage is finite by default so
    the simple water balance closes (an infinite ice store is an unaccounted source)."""
    hydro_units = hb.HydroUnits(
        land_cover_types=["ground", "glacier"], land_cover_names=["ground", "glacier"]
    )
    hu_csv = tmp_path / "hydro_units.csv"
    hu_csv.write_text(
        "id,elevation,area_ground,area_glacier\n"
        "-,m,m^2,m^2\n"
        "1,1000,1000000,0\n"
        "2,2500,300000,700000\n"
    )
    hydro_units.load_from_csv(
        hu_csv,
        column_elevation="elevation",
        columns_areas={"ground": "area_ground", "glacier": "area_glacier"},
    )
    forcing = _load_forcing(hydro_units, _meteo_csv_seasonal(tmp_path, n_days, P, PET))

    model = models.HBV96(
        land_cover_names=["ground", "glacier"],
        land_cover_types=["ground", "glacier"],
        record_all=True,
        glacier_infinite_storage=infinite_storage,
    )
    parameters = model.generate_parameters()
    parameters.set_values(params or _PARAMS_GROUND_GLACIER)

    end_date = (_START + timedelta(days=n_days - 1)).strftime("%Y-%m-%d")
    out = tmp_path / "out"
    out.mkdir()
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(out),
        start_date=_START.strftime("%Y-%m-%d"),
        end_date=end_date,
    )
    model.run(parameters=parameters, forcing=forcing)
    model.dump_outputs(str(out))
    results = hb.Results(str(out / "results.nc"))
    return model, forcing, results


def test_hbv96_glacier_exposes_aliases():
    parameters = models.HBV96(
        land_cover_names=["ground", "glacier"],
        land_cover_types=["ground", "glacier"],
    ).generate_parameters()
    for name in ("a_ice", "k_snow", "k_ice"):
        assert parameters.has(name), f"glacier alias {name!r} not found"


def test_hbv96_glacier_a_snow_lt_a_ice_constraint():
    parameters = models.HBV96(
        land_cover_names=["ground", "glacier"],
        land_cover_types=["ground", "glacier"],
    ).generate_parameters()
    # a_snow (cfmax) > a_ice must violate the constraint.
    parameters.set_values(dict(_PARAMS_GROUND_GLACIER, cfmax=8.0, a_ice=4.0))
    assert not parameters.constraints_satisfied()


def test_hbv96_glacier_water_balance_closes(tmp_path):
    """Glacier-area rain + snowmelt feed a linear reservoir draining to the outlet; with
    a finite ice store (no unaccounted melt source) the catchment balance must close."""
    model, forcing, _ = _run_ground_glacier(tmp_path)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_hbv96_glacier_brick_absent_where_no_glacier(tmp_path):
    """The glacier-free base variant is used by the all-ground unit (no glacier brick),
    while the ground+glacier unit uses the with-glacier variant. Two variants are in
    use, and the glacier brick exists exactly where the glacier cover is present."""
    _, _, results = _run_ground_glacier(tmp_path)
    structure_ids = results.get_hydro_units_structure_ids()
    assert len(set(structure_ids.tolist())) == 2
    glacier_areas = results.get_land_cover_areas("glacier")
    glacier_state = results.get_hydro_units_values("glacier:water_content")
    for i in range(len(results.hydro_units_ids)):
        has_glacier = not bool(np.all(np.isnan(glacier_areas[i, :])))
        brick_present = not bool(np.all(np.isnan(glacier_state[i, :])))
        assert has_glacier == brick_present


def test_hbv96_total_swe_aggregates_land_covers(tmp_path):
    """get_total_swe combines the per-land-cover snowpacks into a unit-average SWE
    depth while keeping the hydro unit dimension. The all-ground unit (no glacier) must
    equal its ground snowpack, and the catchment area-weighted mean of the per-unit
    totals must match get_mean_swe."""
    _, _, results = _run_ground_glacier(tmp_path)
    total_swe = results.get_total_swe()
    ground_swe = results.get_hydro_units_values("ground_snowpack:snow_content")

    # Shape is (units, time), matching the per-unit component arrays.
    assert total_swe.shape == ground_swe.shape

    # Unit 0 is all ground (no glacier): its total SWE is just the ground snowpack.
    np.testing.assert_allclose(total_swe[0, :], ground_swe[0, :], rtol=1e-9)

    # Area-weighting the per-unit totals over the catchment recovers get_mean_swe.
    unit_areas = np.nan_to_num(results.results.hydro_units_areas.to_numpy())[:, None]
    mean_from_total = np.nansum(total_swe * unit_areas, axis=0) / unit_areas.sum(axis=0)
    np.testing.assert_allclose(mean_from_total, results.get_mean_swe(), rtol=1e-9)


def test_hbv96_total_swe_handles_snowless_land_cover(tmp_path):
    """A land cover without a snowpack (a lake) must not raise: it contributes zero SWE
    over its area. The all-ground unit keeps its ground snowpack; the all-lake unit
    averages to zero SWE."""
    _, _, results = _run_ground_lake(tmp_path, PET=0.5)
    total_swe = results.get_total_swe()
    ground_swe = results.get_hydro_units_values("ground_snowpack:snow_content")

    np.testing.assert_allclose(total_swe[0, :], ground_swe[0, :], rtol=1e-9)
    np.testing.assert_allclose(total_swe[1, :], 0.0, atol=1e-12)


def test_hbv96_total_swe_date_slicing(tmp_path):
    """Date selection slices the land-cover areas consistently with the SWE values, so a
    sub-range matches the corresponding slice of the full series, and a single date
    returns a per-unit snapshot."""
    _, _, results = _run_ground_glacier(tmp_path)
    full = results.get_total_swe()
    times = results.results.time.to_numpy()
    start = np.datetime_as_string(times[10], unit="D")
    end = np.datetime_as_string(times[20], unit="D")

    sub = results.get_total_swe(start, end)
    assert sub.shape == (full.shape[0], 11)
    np.testing.assert_allclose(sub, full[:, 10:21], rtol=1e-9)

    snapshot = results.get_total_swe(start)
    assert snapshot.shape == (full.shape[0],)
    np.testing.assert_allclose(snapshot, full[:, 10], rtol=1e-9)


def test_hbv96_glacier_infinite_storage_adds_discharge(tmp_path):
    """An infinite ice store lets the glacier melt ice on top of the precipitation it
    receives, so it yields more discharge than a finite (depletable) store."""
    finite, _, _ = _run_ground_glacier(_subdir(tmp_path, "fin"), infinite_storage=False)
    infinite, _, _ = _run_ground_glacier(
        _subdir(tmp_path, "inf"), infinite_storage=True
    )
    assert infinite.get_total_outlet_discharge() > finite.get_total_outlet_discharge()
