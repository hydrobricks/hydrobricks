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
