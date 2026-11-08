"""GR4J model tests — processes, water balance, and reference verification.

The reference-verification tests use the exact same forcing, parameters and
expected outlet discharge as the C++ tests (core/tests/src/ModelGR4JTest.cpp).
Python and C++ share the same compute core, so with identical inputs they must
produce identical results; this keeps a single shared set of references.
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

# Forcing shared with the C++ ModelGR4JTest fixture: two dry days, seven wet
# days at 50 mm/d, then dry; PET constant at 1 mm/d.
_CPP_PRECIP = [
    0.0,
    0.0,
    50.0,
    50.0,
    50.0,
    50.0,
    50.0,
    50.0,
    50.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
]
_CPP_PET = [1.0] * len(_CPP_PRECIP)

# Expected outlet discharge (mm), copied verbatim from the C++ ModelGR4JTest
# reference tests. Same core + forcing + parameters → identical results.
_CPP_EXPECTED_NO_EXCHANGE = [
    0.0,
    0.0,
    0.003814,
    0.043580,
    0.203829,
    0.548403,
    1.074379,
    2.015970,
    5.228185,
    11.837592,
    7.648482,
    4.872998,
    3.672856,
    3.060546,
    2.629808,
]
_CPP_EXPECTED_POS_EXCHANGE = [
    0.0,
    0.0,
    0.012725,
    0.136420,
    0.573964,
    1.373628,
    3.260256,
    11.690935,
    29.438060,
    33.338379,
    14.274200,
    8.309123,
    6.095194,
    4.869705,
    4.080997,
]
_CPP_EXPECTED_NEG_EXCHANGE = [
    0.0,
    0.0,
    0.012725,
    0.136420,
    0.573950,
    1.371315,
    3.193552,
    10.875901,
    25.635053,
    26.849073,
    9.237705,
    5.488689,
    3.862539,
    2.968519,
    2.407908,
]


def _hu_csv(tmp_path: Path) -> Path:
    """Write a single-HU CSV (elevation 1000 m, area 1 km²)."""
    p = tmp_path / "hydro_units.csv"
    p.write_text("id,elevation,area\n-,m,m^2\n1,1000,1000000\n")
    return p


def _meteo_csv_series(tmp_path: Path, precip, pet) -> Path:
    """Write a meteo CSV from per-day precipitation and PET series."""
    lines = ["date,precip(mm/day),pet(mm/day)"]
    for i, (p, e) in enumerate(zip(precip, pet)):
        d = _START + timedelta(days=i)
        lines.append(f"{d.strftime('%d/%m/%Y')},{p:.6f},{e:.6f}")
    path = tmp_path / "meteo.csv"
    path.write_text("\n".join(lines) + "\n")
    return path


def _meteo_csv(tmp_path: Path, n_days: int, P: float, PET: float) -> Path:
    """Write a meteo CSV with constant daily P and PET."""
    return _meteo_csv_series(tmp_path, [P] * n_days, [PET] * n_days)


def _load_forcing(hydro_units, meteo_path: Path) -> hb.Forcing:
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        meteo_path,
        column_time="date",
        time_format="%d/%m/%Y",
        content={"precipitation": "precip(mm/day)", "pet": "pet(mm/day)"},
    )
    # ref_elevation=1000 matches the single HU elevation; gradient=0 → no correction
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=1000, gradient=0.0
    )
    forcing.spatialize_from_station_data(variable="pet")
    return forcing


def _run_model(
    tmp_path,
    meteo_path,
    n_days,
    *,
    X1,
    X2,
    X3,
    X4,
    discrete=True,
    record_all=False,
    lumped_runoff=False,
) -> tuple:
    """Build and run a GR4J model on a given meteo file; return (model, forcing)."""
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        _hu_csv(tmp_path), column_elevation="elevation", column_area="area"
    )
    forcing = _load_forcing(hydro_units, meteo_path)

    model = models.GR4J(
        discrete=discrete, record_all=record_all, lumped_runoff=lumped_runoff
    )
    parameters = model.generate_parameters()
    parameters.set_values({"X1": X1, "X2": X2, "X3": X3, "X4": X4})

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
    X1=350.0,
    X2=0.0,
    X3=90.0,
    X4=1.7,
    P=5.0,
    PET=3.0,
    n_days=_N_2Y,
    record_all=False,
) -> tuple:
    """Run GR4J with constant forcing; return (model, forcing)."""
    meteo = _meteo_csv(tmp_path, n_days, P, PET)
    return _run_model(
        tmp_path, meteo, n_days, X1=X1, X2=X2, X3=X3, X4=X4, record_all=record_all
    )


def _q(tmp_path, **kwargs) -> np.ndarray:
    """Convenience: run with constant forcing and return the discharge array."""
    model, _ = _run(tmp_path, **kwargs)
    return model.get_outlet_discharge()


def _q_cpp_forcing(
    tmp_path, *, X1, X2, X3, X4, discrete=True, lumped_runoff=False
) -> np.ndarray:
    """Run GR4J on the shared C++ test forcing; return the daily discharge."""
    meteo = _meteo_csv_series(tmp_path, _CPP_PRECIP, _CPP_PET)
    model, _ = _run_model(
        tmp_path,
        meteo,
        len(_CPP_PRECIP),
        X1=X1,
        X2=X2,
        X3=X3,
        X4=X4,
        discrete=discrete,
        lumped_runoff=lumped_runoff,
    )
    return model.get_outlet_discharge()


# ---------------------------------------------------------------------------
# A — Instantiation
# ---------------------------------------------------------------------------


def test_gr4j_instantiation():
    assert models.GR4J().name == "gr4j"


def test_gr4j_continuous_mode_pins_explicit_solver():
    # The continuous (discrete=False) formulation must default to an explicit
    # solver: the sequential solvers have no transit lag and shift the GR4J
    # hydrograph too early. The default (discrete=True) mode keeps the global
    # default, and an explicit solver choice is always respected.
    global_default = models.Socont().solver
    assert models.GR4J(discrete=False).solver == "heun_explicit"
    assert models.GR4J(discrete=True).solver == global_default
    assert (
        models.GR4J(discrete=False, solver="crank_nicolson").solver == "crank_nicolson"
    )


def test_gr4j_generate_parameters_contains_all_four():
    parameters = models.GR4J().generate_parameters()
    for name in ("X1", "X2", "X3", "X4"):
        assert parameters.has(name), f"parameter {name!r} not found"


def test_gr4j_parameter_aliases_resolve():
    parameters = models.GR4J().generate_parameters()
    for alias in ("x2", "x3", "x4"):
        assert parameters.has(alias), f"alias {alias!r} not found"


def test_gr4j_x1_transform_attached_and_used():
    import math

    pytest.importorskip("spotpy")
    parameters = models.GR4J().generate_parameters()

    # The real value goes to C++; the transformed value is log of it.
    parameters.set_values({"X1": 100})
    assert parameters.get("X1") == 100
    assert parameters.get_transformed("X1") == pytest.approx(math.log(100))

    # X1 uses airGR log/exp, so its default lower bound is raised to a positive value.
    parameters.allow_changing = ["X1"]
    spotpy_params = parameters.get_for_spotpy()
    assert spotpy_params[0].minbound == pytest.approx(math.log(1.0), abs=1e-3)
    assert spotpy_params[0].maxbound == pytest.approx(math.log(3000.0), abs=1e-3)


def test_gr4j_transforms_round_trip():
    """X2 (asinh), X3 (log) and X4 (GR4J spreadsheet) map real <-> transformed."""
    import math

    parameters = models.GR4J().generate_parameters()
    parameters.set_values({"X2": -2.0, "X3": 100.0, "X4": 1.7})

    # Real values are preserved (what goes to C++).
    assert parameters.get("X2") == -2.0
    assert parameters.get("X3") == 100.0
    assert parameters.get("X4") == 1.7

    # Transformed values follow the parameter mappings.
    assert parameters.get_transformed("X2") == pytest.approx(math.asinh(-2.0))
    assert parameters.get_transformed("X3") == pytest.approx(math.log(100.0))
    assert parameters.get_transformed("X4") == pytest.approx(math.log(1.7 - 0.5))

    # Setting a transformed value back-transforms to the expected real value.
    parameters.set_values({"X2": math.asinh(3.0)}, transformed=True)
    assert parameters.get("X2") == pytest.approx(3.0)
    parameters.set_values({"X3": math.log(250.0)}, transformed=True)
    assert parameters.get("X3") == pytest.approx(250.0)
    # X4 real = exp(t) + 0.5; t = log(2.5 - 0.5) -> back to 2.5.
    parameters.set_values({"X4": math.log(2.5 - 0.5)}, transformed=True)
    assert parameters.get("X4") == pytest.approx(2.5)


def test_gr4j_transformed_spotpy_bounds():
    import math

    pytest.importorskip("spotpy")
    parameters = models.GR4J().generate_parameters()
    parameters.allow_changing = ["X2", "X3", "X4"]
    sp = {p.name: p for p in parameters.get_for_spotpy()}

    # X2 in [-10, 5] -> asinh bounds (monotonic increasing).
    assert sp["X2"].minbound == pytest.approx(math.asinh(-10.0), abs=1e-3)
    assert sp["X2"].maxbound == pytest.approx(math.asinh(5.0), abs=1e-3)
    # X3 in [1, 500] -> log bounds.
    assert sp["X3"].minbound == pytest.approx(math.log(1.0), abs=1e-3)
    assert sp["X3"].maxbound == pytest.approx(math.log(500.0), abs=1e-3)
    # X4 in [0.51, 4] (lower bound raised for the log) -> log(X4 - 0.5) bounds.
    assert sp["X4"].minbound == pytest.approx(math.log(0.51 - 0.5), abs=1e-3)
    assert sp["X4"].maxbound == pytest.approx(math.log(4.0 - 0.5), abs=1e-3)


# ---------------------------------------------------------------------------
# B — Water balance
# ---------------------------------------------------------------------------


def test_gr4j_water_balance_closes(tmp_path):
    """With X2=0 (no groundwater exchange) the model must close its water balance."""
    model, forcing = _run(tmp_path, X2=0.0, record_all=True)

    precip = forcing.get_total_precipitation()
    discharge = model.get_total_outlet_discharge()
    et = model.get_total_et()
    storage_change = model.get_total_water_storage_changes()
    # No snow → snow_change = 0
    balance = discharge + et + storage_change - precip
    assert balance == pytest.approx(0, abs=1e-6)


# ---------------------------------------------------------------------------
# C — Reference verification (shared with core/tests/src/ModelGR4JTest.cpp)
# Forcing: _CPP_PRECIP / _CPP_PET; initial state S=0, R=0.
# ---------------------------------------------------------------------------


def test_gr4j_reference_no_exchange(tmp_path):
    """X2=0: pure GR4J routing — must match the C++ reference exactly."""
    q = _q_cpp_forcing(tmp_path, X1=300, X2=0.0, X3=100, X4=2.0)
    np.testing.assert_allclose(q, _CPP_EXPECTED_NO_EXCHANGE, atol=5e-7)


def test_gr4j_reference_positive_exchange(tmp_path):
    """X2>0: routing store receives additional groundwater recharge."""
    q = _q_cpp_forcing(tmp_path, X1=200, X2=2.0, X3=90, X4=1.7)
    np.testing.assert_allclose(q, _CPP_EXPECTED_POS_EXCHANGE, atol=6e-7)


def test_gr4j_reference_negative_exchange(tmp_path):
    """X2<0: routing store loses water; QD is clamped to ≥ 0."""
    q = _q_cpp_forcing(tmp_path, X1=200, X2=-3.0, X3=90, X4=1.7)
    np.testing.assert_allclose(q, _CPP_EXPECTED_NEG_EXCHANGE, atol=7e-7)


def test_gr4j_solver_close_to_reference(tmp_path):
    """Solver-based GR4J (discrete=False): integrating the production store and
    routing as a continuous ODE differs from the exact discrete map, so only a
    loose sanity bound applies (within ~a factor of two of the discrete result)."""
    q = _q_cpp_forcing(tmp_path, X1=300, X2=0.0, X3=100, X4=2.0, discrete=False)
    assert np.all(q >= 0.0)
    assert not np.any(np.isnan(q))
    np.testing.assert_allclose(q, _CPP_EXPECTED_NO_EXCHANGE, rtol=1.0, atol=1e-6)


# ---------------------------------------------------------------------------
# D — Limit conditions
# ---------------------------------------------------------------------------


def test_gr4j_zero_precipitation_no_negative_q(tmp_path):
    """With P=0, Q must stay ≥ 0 and contain no NaN."""
    q = _q(tmp_path, P=0.0, PET=3.0)
    assert np.all(q >= 0.0), "negative discharge with zero precipitation"
    assert not np.any(np.isnan(q))


def test_gr4j_zero_pet_produces_discharge(tmp_path):
    """With PET=0, all precipitation must eventually produce runoff (Q > 0)."""
    q = _q(tmp_path, P=5.0, PET=0.0)
    assert np.all(q >= 0.0)
    assert np.sum(q) > 0.0, "no discharge despite non-zero precipitation"


def test_gr4j_min_x4_does_not_crash(tmp_path):
    """X4=0.51 → n1=1, n2=2: minimum useful UH length; must not crash."""
    q = _q(tmp_path, X4=0.51)
    assert np.all(q >= 0.0)
    assert not np.any(np.isnan(q))


def test_gr4j_large_negative_x2_clamps_qd(tmp_path):
    """Large negative X2 must never produce negative Q (QD is clamped to ≥ 0)."""
    q = _q(tmp_path, X2=-5.0)
    assert np.all(q >= 0.0), "negative discharge with large negative X2"


def test_gr4j_small_x3_stays_non_negative(tmp_path):
    """Very small routing capacity (X3=1) must not crash and Q must be ≥ 0."""
    q = _q(tmp_path, X3=1.0)
    assert np.all(q >= 0.0)
    assert not np.any(np.isnan(q))


# ---------------------------------------------------------------------------
# E — Runoff routine lumped over the subbasin (snow per hydro unit)
# ---------------------------------------------------------------------------


def _two_units_csv(tmp_path: Path) -> Path:
    """Two hydro units of different elevation and area (1 and 3 km²)."""
    p = tmp_path / "hydro_units_2.csv"
    p.write_text("id,elevation,area\n-,m,m^2\n1,500,1000000\n2,1500,3000000\n")
    return p


def _meteo_csv_with_temperature(tmp_path: Path, n_days: int) -> Path:
    """Precipitation, PET and a seasonal temperature (snow in winter at altitude),
    with its daily minimum and maximum (CemaNeige splits the precipitation of the
    hydro units below 1500 m by them)."""
    lines = ["date,precip(mm/day),pet(mm/day),temp(C),tmin(C),tmax(C)"]
    for i in range(n_days):
        d = _START + timedelta(days=i)
        precip = 8.0 if i % 4 == 0 else 0.0
        pet = 2.0 + 1.5 * np.sin(2 * np.pi * (i - 100) / 365)
        temp = 5.0 + 10.0 * np.sin(2 * np.pi * (i - 100) / 365)
        lines.append(
            f"{d.strftime('%d/%m/%Y')},{precip:.6f},{pet:.6f},{temp:.6f},"
            f"{temp - 4:.6f},{temp + 4:.6f}"
        )
    path = tmp_path / "meteo_t.csv"
    path.write_text("\n".join(lines) + "\n")
    return path


def _run_two_units(tmp_path: Path, n_days: int, X2=-1.0, **model_options) -> tuple:
    """GR4J on two hydro units, the precipitation increasing with elevation."""
    tmp_path.mkdir(parents=True, exist_ok=True)
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        _two_units_csv(tmp_path), column_elevation="elevation", column_area="area"
    )
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        _meteo_csv_with_temperature(tmp_path, n_days),
        column_time="date",
        time_format="%d/%m/%Y",
        content={
            "precipitation": "precip(mm/day)",
            "pet": "pet(mm/day)",
            "temperature": "temp(C)",
            "temperature_min": "tmin(C)",
            "temperature_max": "tmax(C)",
        },
    )
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=1000, gradient=0.05
    )
    forcing.spatialize_from_station_data(variable="pet")
    for variable in ("temperature", "temperature_min", "temperature_max"):
        forcing.spatialize_from_station_data(
            variable=variable,
            method="additive_elevation_gradient",
            ref_elevation=1000,
            gradient=-0.6,
        )

    model = models.GR4J(record_all=True, **model_options)
    parameters = model.generate_parameters()
    parameters.set_values({"X1": 300, "X2": X2, "X3": 90, "X4": 1.7})
    if model_options.get("snow_melt_process") == "melt:cemaneige":
        parameters.set_values({"Kf": 3.5, "Cn": 300})
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(tmp_path),
        start_date=_START.strftime("%Y-%m-%d"),
        end_date=(_START + timedelta(days=n_days - 1)).strftime("%Y-%m-%d"),
    )
    model.run(parameters=parameters, forcing=forcing)
    return model, forcing


@pytest.mark.parametrize(
    "X1, X2, X3, X4, expected",
    [
        (300, 0.0, 100, 2.0, _CPP_EXPECTED_NO_EXCHANGE),
        (200, 2.0, 90, 1.7, _CPP_EXPECTED_POS_EXCHANGE),
        (200, -3.0, 90, 1.7, _CPP_EXPECTED_NEG_EXCHANGE),
    ],
)
def test_gr4j_lumped_runoff_single_unit_matches_reference(
    tmp_path, X1, X2, X3, X4, expected
):
    """On a single hydro unit, lumping the runoff routine changes nothing."""
    q = _q_cpp_forcing(tmp_path, X1=X1, X2=X2, X3=X3, X4=X4, lumped_runoff=True)
    np.testing.assert_allclose(q, expected, atol=7e-7)


def test_gr4j_lumped_runoff_equals_gr4j_on_mean_forcing(tmp_path):
    """Without snow, the lumped routine on two units is GR4J on the area-weighted
    mean precipitation and PET."""
    n_days = 200
    model, forcing = _run_two_units(tmp_path / "two", n_days, lumped_runoff=True)
    q_lumped = model.get_outlet_discharge()

    series = dict(zip(forcing.data2D.data_name, forcing.data2D.data))
    weights = np.array([0.25, 0.75])
    precip = np.asarray(series["p"], float) @ weights
    pet = np.asarray(series["pet"], float) @ weights
    assert np.ptp(np.asarray(series["p"], float)[0]) > 0

    meteo = _meteo_csv_series(tmp_path, precip, pet)
    single, _ = _run_model(tmp_path, meteo, n_days, X1=300, X2=-1.0, X3=90, X4=1.7)
    np.testing.assert_allclose(q_lumped, single.get_outlet_discharge(), atol=1e-6)


def test_gr4j_lumped_runoff_differs_from_per_unit_routine(tmp_path):
    """The per-unit stores are non-linear, so lumping them changes the response."""
    n_days = 200
    lumped, _ = _run_two_units(tmp_path / "lumped", n_days, lumped_runoff=True)
    per_unit, _ = _run_two_units(tmp_path / "per_unit", n_days)
    q_lumped = lumped.get_outlet_discharge()
    q_per_unit = per_unit.get_outlet_discharge()
    assert np.max(np.abs(q_lumped - q_per_unit)) > 1e-3


def test_gr4j_lumped_runoff_with_snow_closes_water_balance(tmp_path):
    """Snow on the hydro units, runoff routine on the subbasin: with no groundwater
    exchange (X2=0) the water balance closes."""
    model, forcing = _run_two_units(
        tmp_path, _N_2Y, X2=0.0, lumped_runoff=True, snow_melt_process="melt:cemaneige"
    )
    q = model.get_outlet_discharge()
    assert np.all(q >= 0.0)
    assert not np.any(np.isnan(q))
    assert model.get_total_snow_storage_changes() != 0 or np.any(q > 0)
    balance = (
        model.get_total_outlet_discharge()
        + model.get_total_et()
        + model.get_total_water_storage_changes()
        + model.get_total_snow_storage_changes()
        - forcing.get_total_precipitation()
    )
    assert balance == pytest.approx(0, abs=1e-5)


def test_gr4j_cemaneige_without_tmin_tmax_raises(tmp_path):
    """Below 1500 m, CemaNeige needs the daily minimum and maximum temperature: a
    forcing without them is reported instead of being read as missing data."""
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        _two_units_csv(tmp_path), column_elevation="elevation", column_area="area"
    )
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        _meteo_csv_with_temperature(tmp_path, 50),
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
    forcing.spatialize_from_station_data(variable="pet")
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=1000, gradient=-0.6
    )

    model = models.GR4J(snow_melt_process="melt:cemaneige")
    parameters = model.generate_parameters()
    parameters.set_values(
        {"X1": 300, "X2": 0, "X3": 90, "X4": 1.7, "Kf": 3.5, "Cn": 300}
    )
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(tmp_path),
        start_date="1981-01-01",
        end_date="1981-02-19",
    )
    with pytest.raises(hb.ModelError):
        model.run(parameters=parameters, forcing=forcing)
