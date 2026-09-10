"""Sub-daily time steps.

The model parameters and process rates are expressed per day whatever the computation
step, and the forcing is given as an amount per step. These tests pin that contract: a
daily step and a 24-hour step must be the same run, refining the step must converge
rather than drift, and the water balance must close at any step.
"""

from __future__ import annotations

from datetime import date, timedelta
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

import hydrobricks as hb
import hydrobricks.models as models

_START = date(1981, 1, 1)
_N_DAYS = 200

_PARAMS = {
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

_PRECIP_PER_DAY = 5.0
_PET_PER_DAY = 1.5


def _run(tmp_path: Path, hours_per_step: int, n_days: int = _N_DAYS):
    """Run PREVAH with the same daily information split into steps of that length.

    The forcing is an amount per step, so the precipitation and PET of a day are shared
    between its steps and the temperature is held at the daily value. Every step length
    therefore sees the same daily totals.
    """
    sub = tmp_path / f"s{hours_per_step:02d}"
    sub.mkdir(parents=True, exist_ok=True)

    hu_csv = sub / "hydro_units.csv"
    hu_csv.write_text("id,elevation,area\n-,m,m^2\n1,1000,1000000\n")
    units = hb.HydroUnits()
    units.load_from_csv(hu_csv, column_elevation="elevation", column_area="area")

    n_steps = (n_days * 24) // hours_per_step
    stamps = pd.date_range(
        pd.Timestamp(_START), periods=n_steps, freq=f"{hours_per_step}h"
    )
    doy = stamps.dayofyear.to_numpy(dtype=float)
    temp = 5.0 - 12.0 * np.cos(2.0 * np.pi * doy / 365.25)
    share = hours_per_step / 24.0

    lines = ["date,precip,pet,temp"]
    for stamp, t in zip(stamps, temp):
        lines.append(
            f"{stamp.strftime('%Y-%m-%d %H:%M')},"
            f"{_PRECIP_PER_DAY * share:.10f},{_PET_PER_DAY * share:.10f},{t:.6f}"
        )
    meteo = sub / "meteo.csv"
    meteo.write_text("\n".join(lines) + "\n")

    forcing = hb.Forcing(units)
    forcing.load_station_data_from_csv(
        meteo,
        column_time="date",
        time_format="%Y-%m-%d %H:%M",
        content={"precipitation": "precip", "pet": "pet", "temperature": "temp"},
    )
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=1000, gradient=0.0
    )
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=1000, gradient=0.0
    )
    forcing.spatialize_from_station_data(variable="pet")

    model = models.PrevahUniBE(record_all=True)
    parameters = model.generate_parameters()
    parameters.set_values(dict(_PARAMS))

    end = _START + timedelta(days=n_days - 1)
    model.setup(
        spatial_structure=units,
        output_path=str(sub),
        start_date=_START.strftime("%Y-%m-%d"),
        end_date=end.strftime("%Y-%m-%d"),
        time_step=hours_per_step,
        time_step_unit="hour",
    )
    model.run(parameters=parameters, forcing=forcing)
    return model


def _run_daily(tmp_path: Path, n_days: int = _N_DAYS):
    """The same run, declared with the default daily step instead of 24 hours."""
    sub = tmp_path / "daily"
    sub.mkdir(parents=True, exist_ok=True)

    hu_csv = sub / "hydro_units.csv"
    hu_csv.write_text("id,elevation,area\n-,m,m^2\n1,1000,1000000\n")
    units = hb.HydroUnits()
    units.load_from_csv(hu_csv, column_elevation="elevation", column_area="area")

    stamps = pd.date_range(pd.Timestamp(_START), periods=n_days, freq="D")
    doy = stamps.dayofyear.to_numpy(dtype=float)
    temp = 5.0 - 12.0 * np.cos(2.0 * np.pi * doy / 365.25)
    lines = ["date,precip,pet,temp"]
    for stamp, t in zip(stamps, temp):
        lines.append(
            f"{stamp.strftime('%Y-%m-%d %H:%M')},"
            f"{_PRECIP_PER_DAY:.10f},{_PET_PER_DAY:.10f},{t:.6f}"
        )
    meteo = sub / "meteo.csv"
    meteo.write_text("\n".join(lines) + "\n")

    forcing = hb.Forcing(units)
    forcing.load_station_data_from_csv(
        meteo,
        column_time="date",
        time_format="%Y-%m-%d %H:%M",
        content={"precipitation": "precip", "pet": "pet", "temperature": "temp"},
    )
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=1000, gradient=0.0
    )
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=1000, gradient=0.0
    )
    forcing.spatialize_from_station_data(variable="pet")

    model = models.PrevahUniBE(record_all=True)
    parameters = model.generate_parameters()
    parameters.set_values(dict(_PARAMS))
    end = _START + timedelta(days=n_days - 1)
    model.setup(
        spatial_structure=units,
        output_path=str(sub),
        start_date=_START.strftime("%Y-%m-%d"),
        end_date=end.strftime("%Y-%m-%d"),
    )
    model.run(parameters=parameters, forcing=forcing)
    return model


def _balance(model) -> float:
    return (
        model.get_total_outlet_discharge()
        + model.get_total_et()
        + model.get_total_water_storage_changes()
        + model.get_total_snow_storage_changes()
    )


def test_a_24_hour_step_is_the_daily_step(tmp_path):
    """The same step declared in two units must be the very same run."""
    hourly = _run(tmp_path, 24)
    daily = _run_daily(tmp_path)
    assert hourly.get_total_outlet_discharge() == pytest.approx(
        daily.get_total_outlet_discharge(), rel=1e-12
    )
    assert hourly.get_total_et() == pytest.approx(daily.get_total_et(), rel=1e-12)
    assert np.asarray(hourly.get_outlet_discharge()) == pytest.approx(
        np.asarray(daily.get_outlet_discharge()), rel=1e-12
    )


def test_hourly_run_has_the_expected_number_of_steps(tmp_path):
    """The calendar must not lose a step to the rounding of 1/24 of a day."""
    model = _run(tmp_path, 1, n_days=40)
    # From the start to midnight of the last day, inclusive.
    assert len(np.asarray(model.get_outlet_discharge())) == 39 * 24 + 1


def test_water_balance_closes_at_an_hourly_step(tmp_path):
    model = _run(tmp_path, 1, n_days=60)
    # 59 whole days plus the last step of the run.
    steps = 59 * 24 + 1
    expected = _PRECIP_PER_DAY / 24.0 * steps
    assert _balance(model) == pytest.approx(expected, rel=1e-4)


def test_evapotranspiration_does_not_scale_with_the_step(tmp_path):
    """The PET forcing is an amount per step, and the ET processes work in rates.

    Reading the amount as if it were already a daily rate divided the evaporation by
    the number of steps in a day, which is what this pins down.
    """
    hourly = _run(tmp_path, 1, n_days=60)
    daily = _run(tmp_path, 24, n_days=60)
    assert hourly.get_total_et() == pytest.approx(daily.get_total_et(), rel=0.1)


def test_refining_the_step_converges(tmp_path):
    """Halving the step must change the result by less each time, not drift."""
    totals = {
        hours: _run(tmp_path, hours, n_days=120).get_total_outlet_discharge()
        for hours in (24, 12, 6, 3)
    }
    steps = [abs(totals[b] - totals[a]) for a, b in ((24, 12), (12, 6), (6, 3))]
    assert steps[1] < steps[0]
    assert steps[2] < steps[1]


def test_snow_ages_in_days_not_in_steps(tmp_path):
    """The snow albedo decays per day, so its age must not run 24 times faster.

    The PREVAH snow evaporation is albedo-driven, so an age counted in steps would
    show up directly in the total evaporation of a snowy run.
    """
    hourly = _run(tmp_path, 1, n_days=120)
    daily = _run(tmp_path, 24, n_days=120)
    assert hourly.get_total_et() == pytest.approx(daily.get_total_et(), rel=0.1)
