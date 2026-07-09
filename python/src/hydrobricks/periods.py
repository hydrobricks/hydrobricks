"""First-class modelling periods (calibration / validation / simulation).

A :class:`Period` is a named, inclusive date range. :class:`Periods` groups the
canonical split-sample periods (calibration, validation and an optional simulation
span) together with the spin-up policy, so the same object can drive the model setup,
the calibration and the evaluation.

The spin-up replaces the legacy index-based warmup: the model replays the first
year(s) of its own period (unlogged, in the C++ core) to initialize the states, then
restarts at the period start. The whole declared period is therefore evaluated and no
observations are lost.
"""

from __future__ import annotations

import warnings
from datetime import datetime
from typing import TYPE_CHECKING, Iterable

import numpy as np
import pandas as pd

from hydrobricks._exceptions import ConfigurationError, DataError

if TYPE_CHECKING:
    from hydrobricks.evaluation.discharge import DischargeObservations
    from hydrobricks.models.model import Model

DateLike = "str | datetime | pd.Timestamp"


class Period:
    """A named, inclusive date range (e.g. the calibration period).

    Parameters
    ----------
    start, end
        Period bounds (inclusive), as ``'YYYY-MM-DD'`` strings, datetimes or
        Timestamps.
    name
        Optional label (e.g. ``'calibration'``), used in tables and error messages.
    """

    def __init__(
        self,
        start: str | datetime | pd.Timestamp,
        end: str | datetime | pd.Timestamp,
        name: str | None = None,
    ) -> None:
        self.start = pd.Timestamp(start)
        self.end = pd.Timestamp(end)
        self.name = name
        if self.start > self.end:
            raise ConfigurationError(
                f"The period start ({self.start.date()}) is after its end "
                f"({self.end.date()}).",
                item_name=name or "period",
                reason="Invalid period bounds",
            )

    @classmethod
    def coerce(
        cls,
        value: Period | tuple | list,
        name: str | None = None,
    ) -> Period:
        """Build a Period from a Period or a ``(start, end)`` pair."""
        if isinstance(value, Period):
            if name is not None and value.name is None:
                return cls(value.start, value.end, name)
            return value
        if isinstance(value, (tuple, list)) and len(value) == 2:
            return cls(value[0], value[1], name)
        raise ConfigurationError(
            f"A period must be a Period or a (start, end) pair; got {value!r}.",
            item_name=name or "period",
            item_value=value,
            reason="Invalid period specification",
        )

    @property
    def bounds(self) -> tuple[str, str]:
        """The period bounds as ``('YYYY-MM-DD', 'YYYY-MM-DD')`` strings."""
        return (
            self.start.strftime("%Y-%m-%d"),
            self.end.strftime("%Y-%m-%d"),
        )

    @property
    def n_days(self) -> int:
        """The number of days in the (inclusive) period."""
        return (self.end - self.start).days + 1

    def date_range(self) -> pd.DatetimeIndex:
        """The daily date axis of the period."""
        return pd.date_range(self.start, self.end, freq="D")

    def mask(self, time: pd.DatetimeIndex | pd.Series) -> np.ndarray:
        """Boolean mask selecting the period on the given time axis."""
        time = pd.DatetimeIndex(time)
        return np.asarray((time >= self.start) & (time <= self.end))

    def __repr__(self) -> str:
        label = f"'{self.name}', " if self.name else ""
        return f"Period({label}{self.start.date()}..{self.end.date()})"

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, Period):
            return NotImplemented
        return self.start == other.start and self.end == other.end

    def __hash__(self) -> int:
        return hash((self.start, self.end))


def spinup_to_days(
    spinup: int | str,
    start: str | datetime | pd.Timestamp,
) -> int:
    """Convert a spin-up specification to a number of days.

    Parameters
    ----------
    spinup
        Either a number of days (int), or a string like ``'4y'`` (calendar years
        counted from ``start``) or ``'365d'`` (days). ``0`` disables the spin-up.
    start
        The period start the year-based spin-up is counted from.

    Returns
    -------
    The spin-up duration in days.
    """
    if isinstance(spinup, bool):
        raise ConfigurationError(
            "The spinup option must be an int (days) or a string like '4y'.",
            item_name="spinup",
            item_value=spinup,
            reason="Invalid spinup specification",
        )
    if isinstance(spinup, int):
        days = spinup
    elif isinstance(spinup, str):
        spec = spinup.strip().lower()
        try:
            if spec.endswith("y"):
                n_years = int(spec[:-1])
                start = pd.Timestamp(start)
                days = (start + pd.DateOffset(years=n_years) - start).days
            elif spec.endswith("d"):
                days = int(spec[:-1])
            else:
                days = int(spec)
        except ValueError:
            raise ConfigurationError(
                f"Cannot parse the spinup specification '{spinup}'. Use an int "
                "(days) or a string like '4y' or '365d'.",
                item_name="spinup",
                item_value=spinup,
                reason="Invalid spinup specification",
            ) from None
    else:
        raise ConfigurationError(
            "The spinup option must be an int (days) or a string like '4y'.",
            item_name="spinup",
            item_value=spinup,
            reason="Invalid spinup specification",
        )
    if days < 0:
        raise ConfigurationError(
            "The spinup duration cannot be negative.",
            item_name="spinup",
            item_value=spinup,
            reason="Invalid spinup specification",
        )
    return days


class Periods:
    """The canonical modelling periods and the spin-up policy.

    Groups the calibration period, the validation period and the simulation span,
    so a single object can drive the model setup, the calibration and the
    per-period evaluation (split-sample testing).

    Parameters
    ----------
    calibration
        The calibration period, as a :class:`Period` or ``(start, end)`` pair.
    validation
        The validation period. Optional.
    simulation
        The simulation span. Defaults to the union span of the other periods
        (earliest start to latest end).
    spinup
        The spin-up policy applied to a model set up over one of these periods:
        the first years/days of the period are replayed (unlogged) to initialize
        the states before the run restarts at the period start. Either a number
        of days (int) or a string like ``'4y'`` (default: 4 years). A spin-up
        longer than a period is clamped to that period (i.e. the whole period is
        replayed once).

    Examples
    --------
    >>> periods = Periods(
    ...     calibration=('1981-01-01', '2000-12-31'),
    ...     validation=('2001-01-01', '2020-12-31'),
    ...     spinup='4y',
    ... )
    >>> periods.calibration.bounds
    ('1981-01-01', '2000-12-31')
    """

    def __init__(
        self,
        calibration: Period | tuple | list | None = None,
        validation: Period | tuple | list | None = None,
        simulation: Period | tuple | list | None = None,
        spinup: int | str = "4y",
    ) -> None:
        self.calibration = (
            Period.coerce(calibration, "calibration") if calibration else None
        )
        self.validation = (
            Period.coerce(validation, "validation") if validation else None
        )

        defined = [p for p in (self.calibration, self.validation) if p is not None]
        if simulation is not None:
            self.simulation = Period.coerce(simulation, "simulation")
        elif defined:
            self.simulation = Period(
                min(p.start for p in defined),
                max(p.end for p in defined),
                "simulation",
            )
        else:
            raise ConfigurationError(
                "At least one period (calibration, validation or simulation) "
                "must be provided.",
                item_name="periods",
                reason="No period defined",
            )

        # The evaluation periods must lie within the simulation span, otherwise a
        # full-span run cannot be evaluated on them.
        for period in defined:
            if period.start < self.simulation.start or period.end > self.simulation.end:
                raise ConfigurationError(
                    f"The {period.name} period ({period.start.date()}.."
                    f"{period.end.date()}) is not contained in the simulation "
                    f"span ({self.simulation.start.date()}.."
                    f"{self.simulation.end.date()}).",
                    item_name=period.name,
                    reason="Period outside the simulation span",
                )

        if (
            self.calibration is not None
            and self.validation is not None
            and self.calibration.start <= self.validation.end
            and self.validation.start <= self.calibration.end
        ):
            warnings.warn(
                "The calibration and validation periods overlap "
                f"({self.calibration.start.date()}..{self.calibration.end.date()} "
                f"vs {self.validation.start.date()}..{self.validation.end.date()}); "
                "the validation scores will not be independent.",
                stacklevel=2,
            )

        # Validate the specification eagerly (fail at construction, not at setup).
        self.spinup = spinup
        spinup_to_days(spinup, self.simulation.start)

    @property
    def full_span(self) -> Period:
        """The simulation span (earliest start to latest end)."""
        return self.simulation

    def defined_periods(self) -> dict[str, Period]:
        """The defined periods, keyed by name (calibration/validation/simulation)."""
        named = {
            "calibration": self.calibration,
            "validation": self.validation,
            "simulation": self.simulation,
        }
        return {name: p for name, p in named.items() if p is not None}

    def spinup_days_for(self, period: Period) -> int:
        """The spin-up duration in days for the given period (clamped to it)."""
        days = spinup_to_days(self.spinup, period.start)
        return min(days, period.n_days)

    def __repr__(self) -> str:
        parts = [f"{name}={p}" for name, p in self.defined_periods().items()]
        parts.append(f"spinup={self.spinup!r}")
        return f"Periods({', '.join(parts)})"


def evaluate_periods(
    model: Model,
    observations: DischargeObservations | np.ndarray,
    periods: Periods,
    metrics: Iterable[str] = ("kge_2012",),
    transform=None,
) -> pd.DataFrame:
    """Evaluate a simulation on each declared period (split-sample table).

    The model must have been run over a span covering the periods (typically the
    full span, ``periods.simulation``); each metric is then computed on the date
    slice of every defined period. This is the recommended validation workflow:
    calibrate on the calibration period, re-run the best parameters over the full
    span, and read the calibration/validation scores from this table.

    Parameters
    ----------
    model
        A model that has been ``setup()`` and ``run()``.
    observations
        The observed discharge: a :class:`DischargeObservations` (sliced by its
        own dates) or an array aligned with the simulated series.
    periods
        The periods to evaluate on.
    metrics
        HydroErr metric names (e.g. ``'nse'``, ``'kge_2012'``), or ``'kge_np'`` for
        the non-parametric KGE (requires the optional SPOTPY dependency).
    transform
        Optional discharge transformation applied to the observed and simulated
        series of each period before computing the metrics (e.g. ``'power(0.2)'``
        or ``'log'`` to emphasize low flows). Anything accepted by
        :meth:`DischargeTransform.from_spec
        <hydrobricks.evaluation.transforms.DischargeTransform.from_spec>`. An ``'auto'``
        epsilon is resolved per period, from that period's observations.
        Default: None (untransformed).

    Returns
    -------
    A DataFrame with one row per period and one column per metric.
    """
    from hydrobricks.evaluation.metrics import evaluate
    from hydrobricks.evaluation.transforms import DischargeTransform

    transform = DischargeTransform.from_spec(transform)

    sim = model.get_outlet_discharge()
    time = model.get_recorded_time()

    if isinstance(observations, np.ndarray):
        if len(observations) != len(sim):
            raise DataError(
                f"The observations array ({len(observations)} values) does not "
                f"match the simulated series ({len(sim)} values). Pass a "
                "DischargeObservations to slice by dates instead.",
                data_type="observations",
                reason="Length mismatch",
            )
        obs_time = time
        obs_values = observations
    else:
        obs_time = pd.DatetimeIndex(observations.time)
        obs_values = observations.data[0]

    rows = {}
    for name, period in periods.defined_periods().items():
        if period.start < time[0] or period.end > time[-1]:
            raise ConfigurationError(
                f"The {name} period ({period.start.date()}..{period.end.date()}) "
                f"is not covered by the simulation ({time[0].date()}.."
                f"{time[-1].date()}). Run the model over the full span "
                "(periods.simulation) before evaluating.",
                item_name=name,
                reason="Period not covered by the simulation",
            )
        sim_slice = sim[period.mask(time)]
        obs_slice = obs_values[period.mask(obs_time)]
        if len(obs_slice) != len(sim_slice):
            raise DataError(
                f"The observations cover {len(obs_slice)} days of the {name} "
                f"period but the simulation covers {len(sim_slice)}. The "
                "observations must span the evaluated periods.",
                data_type="observations",
                reason="Observations do not cover the period",
            )
        sim_slice, obs_slice = transform.transform_pair(sim_slice, obs_slice)
        rows[name] = {
            metric: evaluate(sim_slice, obs_slice, metric) for metric in metrics
        }

    return pd.DataFrame.from_dict(rows, orient="index", columns=list(metrics))
