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
    tmp_path, meteo_path, n_days, *, X1, X2, X3, X4, discrete=True, record_all=False
) -> tuple:
    """Build and run a GR4J model on a given meteo file; return (model, forcing)."""
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        _hu_csv(tmp_path), column_elevation="elevation", column_area="area"
    )
    forcing = _load_forcing(hydro_units, meteo_path)

    model = models.GR4J(discrete=discrete, record_all=record_all)
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


def _q_cpp_forcing(tmp_path, *, X1, X2, X3, X4, discrete=True) -> np.ndarray:
    """Run GR4J on the shared C++ test forcing; return the daily discharge."""
    meteo = _meteo_csv_series(tmp_path, _CPP_PRECIP, _CPP_PET)
    model, _ = _run_model(
        tmp_path, meteo, len(_CPP_PRECIP), X1=X1, X2=X2, X3=X3, X4=X4, discrete=discrete
    )
    return model.get_outlet_discharge()


# ---------------------------------------------------------------------------
# A — Instantiation
# ---------------------------------------------------------------------------


def test_gr4j_instantiation():
    assert models.GR4J().name == "gr4j"


def test_gr4j_generate_parameters_contains_all_four():
    parameters = models.GR4J().generate_parameters()
    for name in ("X1", "X2", "X3", "X4"):
        assert parameters.has(name), f"parameter {name!r} not found"


def test_gr4j_parameter_aliases_resolve():
    parameters = models.GR4J().generate_parameters()
    for alias in ("x2", "x3", "x4"):
        assert parameters.has(alias), f"alias {alias!r} not found"


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
