"""GR6J model tests — processes, water balance, and reference verification.

The reference-verification tests use the exact same forcing, parameters and
expected outlet discharge as the C++ tests (core/tests/src/ModelGR6JTest.cpp).
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

# Forcing shared with the C++ ModelGR6JTest fixture: two dry days, seven wet
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

# Reference outlet discharge (mm) from the airGR RunModel_GR6J implementation
# (Pushpalatha et al., 2011), with X1=300, X3=100, X4=2, X5=0.3, X6=4 and the
# shared forcing; initial state S=0, R=0, Rexp=0. The three cases differ only by
# the groundwater exchange coefficient X2. hydrobricks shares the same compute
# core in C++ and Python, so both must reproduce these to ~1e-5.
_AIRGR_NO_EXCHANGE = [  # X2 = 0
    2.772589,
    1.621860,
    1.161424,
    1.005298,
    1.249373,
    2.151582,
    4.214921,
    7.687066,
    11.521525,
    12.829463,
    5.323870,
    3.072099,
    2.322168,
    2.032455,
    1.833531,
]
_AIRGR_POS_EXCHANGE = [  # X2 = 2
    2.483828,
    1.340565,
    0.882276,
    0.676503,
    0.686399,
    1.028097,
    2.754801,
    6.301642,
    10.888971,
    12.851495,
    5.795915,
    3.611188,
    2.918015,
    2.678096,
    2.523144,
]
_AIRGR_NEG_EXCHANGE = [  # X2 = -2
    3.683828,
    2.523956,
    2.055770,
    1.908026,
    2.217910,
    3.269898,
    5.447277,
    8.589649,
    11.827181,
    12.701670,
    4.808356,
    2.631865,
    2.088443,
    1.754517,
    1.522078,
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
    X5,
    X6,
    discrete=True,
    record_all=False,
) -> tuple:
    """Build and run a GR6J model on a given meteo file; return (model, forcing)."""
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        _hu_csv(tmp_path), column_elevation="elevation", column_area="area"
    )
    forcing = _load_forcing(hydro_units, meteo_path)

    model = models.GR6J(discrete=discrete, record_all=record_all)
    parameters = model.generate_parameters()
    parameters.set_values({"X1": X1, "X2": X2, "X3": X3, "X4": X4, "X5": X5, "X6": X6})

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
    X5=0.0,
    X6=4.0,
    P=5.0,
    PET=3.0,
    n_days=_N_2Y,
    record_all=False,
) -> tuple:
    """Run GR6J with constant forcing; return (model, forcing)."""
    meteo = _meteo_csv(tmp_path, n_days, P, PET)
    return _run_model(
        tmp_path,
        meteo,
        n_days,
        X1=X1,
        X2=X2,
        X3=X3,
        X4=X4,
        X5=X5,
        X6=X6,
        record_all=record_all,
    )


def _q(tmp_path, **kwargs) -> np.ndarray:
    """Convenience: run with constant forcing and return the discharge array."""
    model, _ = _run(tmp_path, **kwargs)
    return model.get_outlet_discharge()


# ---------------------------------------------------------------------------
# A — Instantiation
# ---------------------------------------------------------------------------


def test_gr6j_instantiation():
    assert models.GR6J().name == "gr6j"


def test_gr6j_generate_parameters_contains_all_six():
    parameters = models.GR6J().generate_parameters()
    for name in ("X1", "X2", "X3", "X4", "X5", "X6"):
        assert parameters.has(name), f"parameter {name!r} not found"


def test_gr6j_parameter_aliases_resolve():
    parameters = models.GR6J().generate_parameters()
    for alias in ("x2", "x3", "x4", "x5", "x6"):
        assert parameters.has(alias), f"alias {alias!r} not found"


def test_gr6j_transforms_round_trip():
    """X5 (asinh) and X6 (log) map real <-> transformed correctly."""
    import math

    parameters = models.GR6J().generate_parameters()
    parameters.set_values({"X5": -1.5, "X6": 4.0})

    assert parameters.get("X5") == -1.5
    assert parameters.get("X6") == 4.0
    assert parameters.get_transformed("X5") == pytest.approx(math.asinh(-1.5))
    assert parameters.get_transformed("X6") == pytest.approx(math.log(4.0))

    parameters.set_values({"X6": math.log(10.0)}, transformed=True)
    assert parameters.get("X6") == pytest.approx(10.0)


# ---------------------------------------------------------------------------
# B — Water balance (X2=0 → no intercatchment exchange)
# ---------------------------------------------------------------------------


def test_gr6j_water_balance_closes(tmp_path):
    """With X2=0 (no groundwater exchange) the model must close its water balance."""
    model, forcing = _run(tmp_path, X2=0.0, record_all=True)

    precip = forcing.get_total_precipitation()
    discharge = model.get_total_outlet_discharge()
    et = model.get_total_et()
    storage_change = model.get_total_water_storage_changes()
    balance = discharge + et + storage_change - precip
    assert balance == pytest.approx(0, abs=1e-6)


# ---------------------------------------------------------------------------
# C — airGR ground-truth verification (X1=300, X3=100, X4=2, X5=0.3, X6=4)
# Shared with core/tests/src/ModelGR6JTest.cpp.
# ---------------------------------------------------------------------------


def _q_airgr(tmp_path, X2: float) -> np.ndarray:
    meteo = _meteo_csv_series(tmp_path, _CPP_PRECIP, _CPP_PET)
    model, _ = _run_model(
        tmp_path,
        meteo,
        len(_CPP_PRECIP),
        X1=300,
        X2=X2,
        X3=100,
        X4=2.0,
        X5=0.3,
        X6=4.0,
    )
    return model.get_outlet_discharge()


def test_gr6j_matches_airgr_no_exchange(tmp_path):
    """X2=0: must reproduce the airGR RunModel_GR6J discharge."""
    np.testing.assert_allclose(_q_airgr(tmp_path, 0.0), _AIRGR_NO_EXCHANGE, atol=1e-5)


def test_gr6j_matches_airgr_positive_exchange(tmp_path):
    """X2=2: threshold exchange + exponential store — matches airGR."""
    np.testing.assert_allclose(_q_airgr(tmp_path, 2.0), _AIRGR_POS_EXCHANGE, atol=1e-5)


def test_gr6j_matches_airgr_negative_exchange(tmp_path):
    """X2=-2: net groundwater loss — matches airGR."""
    np.testing.assert_allclose(_q_airgr(tmp_path, -2.0), _AIRGR_NEG_EXCHANGE, atol=1e-5)


# ---------------------------------------------------------------------------
# D — Limit conditions
# ---------------------------------------------------------------------------


def test_gr6j_zero_precipitation_no_negative_q(tmp_path):
    """With P=0, Q must stay ≥ 0 and contain no NaN."""
    q = _q(tmp_path, P=0.0, PET=3.0)
    assert np.all(q >= 0.0), "negative discharge with zero precipitation"
    assert not np.any(np.isnan(q))


def test_gr6j_zero_pet_produces_discharge(tmp_path):
    """With PET=0, all precipitation must eventually produce runoff (Q > 0)."""
    q = _q(tmp_path, P=5.0, PET=0.0)
    assert np.all(q >= 0.0)
    assert np.sum(q) > 0.0, "no discharge despite non-zero precipitation"


def test_gr6j_negative_exchange_non_negative_q(tmp_path):
    """Large negative X2 must never produce negative Q (branches clamped to ≥ 0)."""
    q = _q(tmp_path, X2=-5.0, X5=-2.0)
    assert np.all(q >= 0.0), "negative discharge with large negative X2"


def test_gr6j_small_x6_stays_non_negative(tmp_path):
    """Minimum exponential-store coefficient (X6=0.05) must not crash; Q ≥ 0."""
    q = _q(tmp_path, X6=0.05)
    assert np.all(q >= 0.0)
    assert not np.any(np.isnan(q))


def test_gr6j_small_x3_stays_non_negative(tmp_path):
    """Very small routing capacity (X3=1) must not crash and Q must be ≥ 0."""
    q = _q(tmp_path, X3=1.0)
    assert np.all(q >= 0.0)
    assert not np.any(np.isnan(q))
