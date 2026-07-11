"""PREVAH model tests — instantiation, options, water balance and behaviour.

The PREVAH structure (Viviroli et al., 2009) is integrated by the ODE solver,
so these tests verify the structure wiring (seasonal degree-day snow routine
with liquid water retention and refreezing, beta-function soil moisture
routine, threshold upper zone, soil-moisture-gated percolation and the
SLOWCOMP three-store groundwater) and the water balance closure rather than
the discrete Fortran reference.
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
    "a_snow_min": 2.0,
    "a_snow_max": 6.0,
    "melt_t_snow": 0.0,
    "fc": 200.0,
    "cu": 0.7,
    "beta": 2.0,
    "k0": 0.5,
    "sgrluz": 20.0,
    "k1": 0.2,
    "cperc": 2.0,
    "slz1max": 20.0,
    "k_gw1": 0.05,
    "k_gw2": 0.01,
    "k_gw3": 0.005,
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
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=1000, gradient=0.0
    )
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=1000, gradient=0.0
    )
    forcing.spatialize_from_station_data(variable="pet")
    return forcing


def _run_model(tmp_path, meteo_path, n_days, params=None, **model_options) -> tuple:
    """Build and run a PREVAH model on a given meteo file; return (model, forcing)."""
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        _hu_csv(tmp_path), column_elevation="elevation", column_area="area"
    )
    forcing = _load_forcing(hydro_units, meteo_path)

    model = models.Prevah(**model_options)
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
    """Run PREVAH with seasonal temperature forcing; return (model, forcing)."""
    meteo = _meteo_csv_seasonal(tmp_path, n_days, P, PET)
    return _run_model(tmp_path, meteo, n_days, params=params, **model_options)


def _balance(model, forcing) -> float:
    precip = forcing.get_total_precipitation()
    discharge = model.get_total_outlet_discharge()
    et = model.get_total_et()
    storage_change = model.get_total_water_storage_changes()
    snow_change = model.get_total_snow_storage_changes()
    return discharge + et + storage_change + snow_change - precip


def _subdir(tmp_path: Path, name: str) -> Path:
    d = tmp_path / name
    d.mkdir()
    return d


def _run_fc(tmp_path, *, areas, fc_global=200.0, fc_spatial=None, n_days=_N_2Y):
    """Run a single-cover ('open') PREVAH over one or more equal-elevation units.

    ``areas`` gives each unit's area (m²). When ``fc_spatial`` (one value per unit) is
    given, the soil field capacity is set per unit from an ``fc`` property (spatial
    parameter); otherwise the global ``fc_global`` applies to every unit."""
    tmp_path.mkdir(parents=True, exist_ok=True)
    hydro_units = hb.HydroUnits(land_cover_types=["open"], land_cover_names=["open"])
    hu_csv = tmp_path / "hydro_units.csv"
    lines = ["id,elevation,area", "-,m,m^2"]
    for i, area in enumerate(areas):
        lines.append(f"{i + 1},1000,{area:.1f}")
    hu_csv.write_text("\n".join(lines) + "\n")
    hydro_units.load_from_csv(hu_csv, column_elevation="elevation", column_area="area")
    if fc_spatial is not None:
        hydro_units.add_property(("fc", "mm"), np.array(fc_spatial, dtype=float))
    forcing = _load_forcing(
        hydro_units, _meteo_csv_seasonal(tmp_path, n_days, 5.0, 1.5)
    )

    model = models.Prevah(
        land_cover_names=["open"], land_cover_types=["open"], record_all=True
    )
    parameters = model.generate_parameters()
    values = dict(_DEFAULT_PARAMS)
    values["fc"] = fc_global
    parameters.set_values(values)
    if fc_spatial is not None:
        parameters.set_spatial("fc", "fc")

    end_date = (_START + timedelta(days=n_days - 1)).strftime("%Y-%m-%d")
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(tmp_path),
        start_date=_START.strftime("%Y-%m-%d"),
        end_date=end_date,
    )
    model.run(parameters=parameters, forcing=forcing)
    return model


# ---------------------------------------------------------------------------
# A — Instantiation and options
# ---------------------------------------------------------------------------


def test_prevah_instantiation():
    assert models.Prevah().name == "prevah"


def test_prevah_generate_parameters_contains_literature_names():
    parameters = models.Prevah().generate_parameters()
    for name in (
        "crmfmin",  # winter degree-day factor (PREVAH CRMFMIN)
        "crmfmax",  # summer degree-day factor (PREVAH CRMFMAX)
        "melt_t_snow",
        "cwh",
        "cfr",
        "fc",
        "cu",  # ET limit (PREVAH CU, = HBV lp)
        "beta",
        "k0",
        "sgrluz",
        "k1",
        "cperc",
        "cu_perc",
        "slz1max",
        "k_gw1",
        "k_gw2",
        "k_gw3",
        "sublimation_pet_factor",
    ):
        assert parameters.has(name), f"parameter {name!r} not found"


def test_prevah_refreezing_requires_degree_day_melt():
    with pytest.raises(hb.ConfigurationError):
        models.Prevah(snow_melt_process="melt:temperature_index")


def test_prevah_hock_melt_without_refreezing_is_accepted():
    model = models.Prevah(
        snow_melt_process="melt:temperature_index",
        snow_refreezing_process=None,
    )
    parameters = model.generate_parameters()
    assert parameters.has("r_snow")
    assert not parameters.has("cfr")


def test_prevah_rain_to_snowpack_requires_water_retention():
    with pytest.raises(hb.ConfigurationError):
        models.Prevah(snow_water_retention_process=None, snow_refreezing_process=None)


def test_prevah_default_glacier_module_is_prevah():
    assert models.Prevah().options["glacier_module"] == "prevah"


def test_prevah_requires_a_soil_cover():
    with pytest.raises(hb.ConfigurationError):
        models.Prevah(land_cover_names=["glacier"], land_cover_types=["glacier"])


# ---------------------------------------------------------------------------
# B — Water balance
# ---------------------------------------------------------------------------


def test_prevah_water_balance_closes(tmp_path):
    model, forcing = _run(tmp_path, record_all=True)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_prevah_water_balance_closes_without_sublimation(tmp_path):
    model, forcing = _run(tmp_path, record_all=True, snow_sublimation_process=None)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_prevah_water_balance_closes_without_rain_to_snowpack(tmp_path):
    model, forcing = _run(tmp_path, record_all=True, rain_to_snowpack=False)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


# ---------------------------------------------------------------------------
# C — Behaviour and limit conditions
# ---------------------------------------------------------------------------


def test_prevah_discharge_non_negative_no_nan(tmp_path):
    model, _ = _run(tmp_path)
    q = model.get_outlet_discharge()
    assert np.all(q >= 0.0)
    assert not np.any(np.isnan(q))


def test_prevah_zero_precipitation_produces_no_discharge(tmp_path):
    model, _ = _run(tmp_path, P=0.0)
    q = model.get_outlet_discharge()
    assert np.sum(q) == pytest.approx(0, abs=1e-8)


def test_prevah_sgrluz_threshold_lowers_the_storm_peak(tmp_path):
    """A larger surface-runoff threshold (SGRLUZ) suppresses the fast Q0 response,
    lowering the storm peak (the volume drains later through interflow and
    percolation)."""

    def _q_storm(sgrluz: float) -> np.ndarray:
        n_days = 60
        precip = [0.0] * n_days
        precip[10:13] = [50.0, 50.0, 50.0]
        meteo = _meteo_csv_series(
            _subdir(tmp_path, f"s{sgrluz:.0f}"), precip, [1.0] * n_days, [10.0] * n_days
        )
        model, _ = _run_model(
            tmp_path.joinpath(f"s{sgrluz:.0f}"),
            meteo,
            n_days,
            params={"sgrluz": sgrluz},
        )
        return model.get_outlet_discharge()

    q_low = _q_storm(0.0)
    q_high = _q_storm(60.0)
    assert np.max(q_high) < np.max(q_low)


def test_prevah_percolation_shuts_off_below_cu_fraction(tmp_path):
    """With the soil moisture kept below cu_perc x FC (dry conditions), the gated
    percolation stays off and the groundwater contribution is negligible: the
    (default) run with a wetter soil yields more baseflow-driven discharge late in
    a dry spell than a run whose percolation threshold sits above the soil state."""
    # Storm followed by a long recession; a high cu_perc (0.89) blocks percolation
    # while a low one (0.05) lets the groundwater fill and sustain the recession.
    n_days = 120
    precip = [0.0] * n_days
    precip[10:16] = [30.0] * 6

    def _recession_q(cu_perc: float) -> float:
        sub = _subdir(tmp_path, f"p{cu_perc:.2f}")
        meteo = _meteo_csv_series(sub, precip, [1.0] * n_days, [10.0] * n_days)
        model, _ = _run_model(
            sub, meteo, n_days, params={"cu_perc": cu_perc, "fc": 200.0}
        )
        q = model.get_outlet_discharge()
        return float(np.sum(q[60:]))  # late recession = groundwater-driven

    assert _recession_q(0.05) > _recession_q(0.89)


def _run_open_wetland(tmp_path, *, wet_fraction, P=5.0, PET=1.5, n_days=_N_2Y):
    """Run PREVAH with an open and a wetland cover (60/40)."""
    hydro_units = hb.HydroUnits(
        land_cover_types=["open", "wetland"], land_cover_names=["open", "wetland"]
    )
    hu_csv = tmp_path / "hydro_units.csv"
    hu_csv.write_text(
        "id,elevation,area_open,area_wetland\n-,m,m^2,m^2\n1,1000,600000,400000\n"
    )
    hydro_units.load_from_csv(
        hu_csv,
        column_elevation="elevation",
        columns_areas={"open": "area_open", "wetland": "area_wetland"},
    )
    forcing = _load_forcing(hydro_units, _meteo_csv_seasonal(tmp_path, n_days, P, PET))

    model = models.Prevah(
        land_cover_names=["open", "wetland"],
        land_cover_types=["open", "wetland"],
        record_all=True,
    )
    parameters = model.generate_parameters()
    values = dict(_DEFAULT_PARAMS)
    values.pop("beta")
    values.update({"beta_open": 2.0, "beta_wetland": 2.0, "wet_fraction": wet_fraction})
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


def test_prevah_wetland_exposes_wet_fraction_alias():
    parameters = models.Prevah(
        land_cover_names=["open", "wetland"],
        land_cover_types=["open", "wetland"],
    ).generate_parameters()
    assert parameters.has("wet_fraction")


def test_prevah_wetland_water_balance_closes(tmp_path):
    model, forcing = _run_open_wetland(tmp_path, wet_fraction=0.7)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_prevah_wetland_only_cover_is_rejected():
    with pytest.raises(hb.ConfigurationError):
        models.Prevah(land_cover_names=["wetland"], land_cover_types=["wetland"])


# ---------------------------------------------------------------------------
# C2 — Forest canopy interception (WP4: Menzel exactness option)
# ---------------------------------------------------------------------------


def _run_open_forest(
    tmp_path, *, ic, ic_monthly=None, n_days=_N_2Y, P=5.0, PET=1.5, **model_options
):
    """Run PREVAH with an open and a forest cover (60/40); ic = canopy capacity.

    When ``ic_monthly`` (12 values) is given, the canopy capacity is set as a
    monthly-varying parameter instead of a constant."""
    hydro_units = hb.HydroUnits(
        land_cover_types=["open", "forest"], land_cover_names=["open", "forest"]
    )
    hu_csv = tmp_path / "hydro_units.csv"
    hu_csv.write_text(
        "id,elevation,area_open,area_forest\n-,m,m^2,m^2\n1,1000,600000,400000\n"
    )
    hydro_units.load_from_csv(
        hu_csv,
        column_elevation="elevation",
        columns_areas={"open": "area_open", "forest": "area_forest"},
    )
    forcing = _load_forcing(hydro_units, _meteo_csv_seasonal(tmp_path, n_days, P, PET))

    model = models.Prevah(
        land_cover_names=["open", "forest"],
        land_cover_types=["open", "forest"],
        record_all=True,
        **model_options,
    )
    parameters = model.generate_parameters()
    values = dict(_DEFAULT_PARAMS)
    values.pop("beta")
    values.update({"beta_open": 2.0, "beta_forest": 2.0, "ic": ic})
    parameters.set_values(values)
    if ic_monthly is not None:
        parameters.set_monthly_values("ic", ic_monthly)

    end_date = (_START + timedelta(days=n_days - 1)).strftime("%Y-%m-%d")
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(tmp_path),
        start_date=_START.strftime("%Y-%m-%d"),
        end_date=end_date,
    )
    model.run(parameters=parameters, forcing=forcing)
    return model, forcing


def test_prevah_canopy_default_is_menzel():
    assert (
        models.Prevah().options["canopy_interception_process"] == "interception:menzel"
    )


def test_prevah_menzel_canopy_water_balance_closes(tmp_path):
    model, forcing = _run_open_forest(tmp_path, ic=3.0)
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_prevah_menzel_intercepts_less_than_threshold(tmp_path):
    """For the same capacity, Menzel filling retains a diminishing fraction of the rain
    (partial throughfall before the canopy is full), so it evaporates less from the
    canopy and yields more discharge than the fill-then-spill threshold store."""
    menzel, _ = _run_open_forest(
        _subdir(tmp_path, "menzel"),
        ic=3.0,
        canopy_interception_process="interception:menzel",
    )
    threshold, _ = _run_open_forest(
        _subdir(tmp_path, "threshold"),
        ic=3.0,
        canopy_interception_process="outflow:threshold",
    )
    assert menzel.get_total_outlet_discharge() > threshold.get_total_outlet_discharge()


def test_prevah_monthly_capacity_matches_constant(tmp_path):
    """A 12-month-constant monthly capacity reproduces the scalar-capacity run."""
    scalar, _ = _run_open_forest(_subdir(tmp_path, "scalar"), ic=3.0)
    monthly, _ = _run_open_forest(
        _subdir(tmp_path, "monthly"), ic=3.0, ic_monthly=[3.0] * 12
    )
    assert monthly.get_total_outlet_discharge() == pytest.approx(
        scalar.get_total_outlet_discharge(), rel=1e-6
    )


def test_prevah_monthly_capacity_water_balance_closes(tmp_path):
    model, forcing = _run_open_forest(
        tmp_path, ic=2.0, ic_monthly=[1, 1, 1, 2, 3, 4, 5, 4, 3, 2, 1, 1]
    )
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_prevah_monthly_capacity_bounded_by_constants(tmp_path):
    """A seasonally varying capacity gives a discharge distinct from the constant
    runs and bounded by the constant runs at the monthly min and max (more canopy
    capacity intercepts and evaporates more, leaving less discharge)."""
    low, _ = _run_open_forest(_subdir(tmp_path, "low"), ic=1.0)
    high, _ = _run_open_forest(_subdir(tmp_path, "high"), ic=5.0)
    months = [1, 1, 1, 1, 1, 5, 5, 5, 5, 1, 1, 1]  # low in winter, high in summer
    var, _ = _run_open_forest(_subdir(tmp_path, "var"), ic=3.0, ic_monthly=months)
    # A constant capacity at the annual mean: the monthly run must differ from it,
    # proving the value truly varies within the year (not just the mean baseline).
    const_mean, _ = _run_open_forest(_subdir(tmp_path, "mean"), ic=sum(months) / 12.0)
    q_low = low.get_total_outlet_discharge()
    q_high = high.get_total_outlet_discharge()
    q_var = var.get_total_outlet_discharge()
    assert q_high < q_var < q_low
    assert q_var != pytest.approx(const_mean.get_total_outlet_discharge())


def test_prevah_spatial_fc_uniform_matches_global(tmp_path):
    """A uniform per-unit fc property reproduces the global-scalar run."""
    spatial = _run_fc(_subdir(tmp_path, "sp"), areas=[1e6], fc_spatial=[200.0])
    glob = _run_fc(_subdir(tmp_path, "gl"), areas=[1e6], fc_global=200.0)
    assert spatial.get_total_outlet_discharge() == pytest.approx(
        glob.get_total_outlet_discharge(), rel=1e-6
    )


def test_prevah_spatial_fc_uses_own_value(tmp_path):
    """Each unit uses its own per-unit fc: a 2-unit [50, 400] catchment equals the
    area-weighted average of single-unit fc=50 and fc=400 runs."""
    q50 = _run_fc(
        _subdir(tmp_path, "u50"), areas=[1e6], fc_spatial=[50.0]
    ).get_total_outlet_discharge()
    q400 = _run_fc(
        _subdir(tmp_path, "u400"), areas=[1e6], fc_spatial=[400.0]
    ).get_total_outlet_discharge()
    two = _run_fc(
        _subdir(tmp_path, "two"), areas=[5e5, 5e5], fc_spatial=[50.0, 400.0]
    ).get_total_outlet_discharge()
    assert two == pytest.approx(0.5 * q50 + 0.5 * q400, rel=1e-6)
    # And a spatially varying catchment differs from the uniform global run.
    assert two != pytest.approx(
        _run_fc(_subdir(tmp_path, "g200"), areas=[1e6]).get_total_outlet_discharge()
    )


def test_prevah_spatial_fc_water_balance_closes(tmp_path):
    model = _run_fc(tmp_path, areas=[5e5, 5e5], fc_spatial=[50.0, 400.0])
    balance = (
        model.get_total_outlet_discharge()
        + model.get_total_et()
        + model.get_total_water_storage_changes()
        + model.get_total_snow_storage_changes()
    )
    # The forcing is a constant 5 mm/d over the whole catchment, so the closure
    # reference is 5 mm/d x the number of days.
    assert balance == pytest.approx(5.0 * _N_2Y, rel=1e-4)


# ---------------------------------------------------------------------------
# D — Glacier (PREVAH module, ice + firn)
# ---------------------------------------------------------------------------

_PARAMS_GLACIER = {
    "a_ice_min": 3.0,
    "a_ice_max": 8.0,
    "k_snow": 0.3,
    "k_ice": 0.4,
}


def _run_open_glacier(
    tmp_path,
    *,
    cover_names,
    cover_types,
    areas,
    params,
    P=5.0,
    PET=1.5,
    n_days=_N_2Y,
    **model_options,
) -> tuple:
    """Run PREVAH on a 2-unit catchment: an all-open unit and a glacierized unit."""
    hydro_units = hb.HydroUnits(
        land_cover_types=cover_types, land_cover_names=cover_names
    )
    hu_csv = tmp_path / "hydro_units.csv"
    header = ",".join(f"area_{name}" for name in cover_names)
    hu_csv.write_text(
        f"id,elevation,{header}\n"
        f"-,m,{','.join(['m^2'] * len(cover_names))}\n"
        f"1,1000,{areas[0]}\n"
        f"2,2500,{areas[1]}\n"
    )
    hydro_units.load_from_csv(
        hu_csv,
        column_elevation="elevation",
        columns_areas={name: f"area_{name}" for name in cover_names},
    )
    forcing = _load_forcing(hydro_units, _meteo_csv_seasonal(tmp_path, n_days, P, PET))

    model = models.Prevah(
        land_cover_names=cover_names,
        land_cover_types=cover_types,
        record_all=True,
        glacier_infinite_storage=model_options.pop("glacier_infinite_storage", False),
        **model_options,
    )
    parameters = model.generate_parameters()
    values = dict(_DEFAULT_PARAMS)
    values.update(params)
    parameters.set_values(values)

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
    return model, forcing


def test_prevah_glacier_exposes_aliases():
    parameters = models.Prevah(
        land_cover_names=["open", "glacier"],
        land_cover_types=["open", "glacier"],
    ).generate_parameters()
    for name in ("a_ice_min", "a_ice_max", "k_snow", "k_ice"):
        assert parameters.has(name), f"glacier alias {name!r} not found"
    assert not parameters.has("k_firn")  # no firn cover declared


def test_prevah_firn_cover_exposes_firn_alias():
    parameters = models.Prevah(
        land_cover_names=["open", "glacier_ice", "glacier_firn"],
        land_cover_types=["open", "glacier", "glacier"],
    ).generate_parameters()
    for name in ("k_snow", "k_ice", "k_firn"):
        assert parameters.has(name), f"glacier alias {name!r} not found"


def test_prevah_glacier_water_balance_closes(tmp_path):
    """With a finite ice store (no unaccounted melt source) the balance closes."""
    model, forcing = _run_open_glacier(
        tmp_path,
        cover_names=["open", "glacier"],
        cover_types=["open", "glacier"],
        areas=["1000000,0", "300000,700000"],
        params=_PARAMS_GLACIER,
    )
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_prevah_firn_to_groundwater_water_balance_closes(tmp_path):
    """The firn melt drains through the firn reservoir into the groundwater store
    (slz1); the catchment balance must still close."""
    params = dict(_PARAMS_GLACIER)
    params.update(
        {
            "a_ice_min_glacier_ice": 3.0,
            "a_ice_max_glacier_ice": 8.0,
            "a_ice_min_glacier_firn": 2.5,
            "a_ice_max_glacier_firn": 7.0,
            "k_firn": 0.02,
        }
    )
    for key in ("a_ice_min", "a_ice_max"):
        params.pop(key)
    model, forcing = _run_open_glacier(
        tmp_path,
        cover_names=["open", "glacier_ice", "glacier_firn"],
        cover_types=["open", "glacier", "glacier"],
        areas=["1000000,0,0", "300000,400000,300000"],
        params=params,
    )
    assert _balance(model, forcing) == pytest.approx(0, abs=1e-6)


def test_prevah_glacier_infinite_storage_adds_discharge(tmp_path):
    """An infinite ice store lets the glacier melt ice on top of the precipitation
    it receives, so it yields more discharge than a finite (depletable) store."""
    common = dict(
        cover_names=["open", "glacier"],
        cover_types=["open", "glacier"],
        areas=["1000000,0", "300000,700000"],
        params=_PARAMS_GLACIER,
    )
    finite, _ = _run_open_glacier(_subdir(tmp_path, "fin"), **common)
    infinite, _ = _run_open_glacier(
        _subdir(tmp_path, "inf"), glacier_infinite_storage=True, **common
    )
    assert infinite.get_total_outlet_discharge() > finite.get_total_outlet_discharge()
