from __future__ import annotations

from typing import TYPE_CHECKING

import numpy as np

if TYPE_CHECKING:
    import pandas as pd

    from hydrobricks.models.model import Model


class AuxiliaryObservation:
    """Base class for an auxiliary calibration/evaluation signal.

    Besides the primary discharge, a model can be evaluated against additional
    observed signals — glacier mass balance, snow cover, ... Each such signal is
    represented by a subclass that knows how to provide the observed values and to
    compute the matching simulated values from a run model. The two are returned as
    **aligned value vectors** (``observed()`` and ``simulated(model)`` must have the
    same length and ordering), which keeps the contract agnostic to whether the
    signal is a time series or a set of per-period / per-band / per-(date, unit)
    targets.

    A signal also carries how it should be used during calibration:

    - ``mode='objective'`` contributes a ``weight``-scaled goodness-of-fit
      (``metric``) term to the combined objective;
    - ``mode='constraint'`` acts as a behavioural pass/fail filter — a run is
      rejected when the mean absolute error exceeds ``tolerance`` (absolute, in the
      signal's units) or, alternatively, ``relative_tolerance`` times the mean
      absolute observed value. Exactly one of the two must be set.

    Attributes
    ----------
    metric : str
        HydroErr metric name used for the objective term (default ``'rmse'``).
    weight : float
        Weight of this term in the combined ``'weighted'`` score (default 1.0).
    mode : str
        ``'objective'`` or ``'constraint'`` (default ``'objective'``).
    tolerance : float or None
        Maximum allowed mean absolute error for ``'constraint'`` mode, in the
        signal's units. Mutually exclusive with ``relative_tolerance``.
    relative_tolerance : float or None
        Maximum allowed mean absolute error for ``'constraint'`` mode, expressed as
        a fraction of the mean absolute observed value (e.g. ``0.1`` for 10%).
        Mutually exclusive with ``tolerance``.
    requires_recording : bool
        Whether computing the simulated values needs the model run with
        ``record_all=True`` (default True).
    """

    metric: str = "rmse"
    weight: float = 1.0
    mode: str = "objective"
    tolerance: float | None = None
    relative_tolerance: float | None = None
    requires_recording: bool = True

    def observed(self) -> np.ndarray:
        """Return the observed values as a 1D array."""
        raise NotImplementedError

    def simulated(self, model: Model) -> np.ndarray:
        """Return the simulated values matching :meth:`observed`, from a run model.

        The model must already have been run (and recorded, if
        ``requires_recording``). The returned array must align 1:1 with
        :meth:`observed`; entries that cannot be evaluated should be NaN.
        """
        raise NotImplementedError

    def restrict_to_period(
        self,
        start: str | pd.Timestamp | None,
        end: str | pd.Timestamp | None,
    ) -> None:
        """Restrict the observations to ``[start, end]`` (default: no-op)."""
        return None

    def __len__(self) -> int:
        return len(self.observed())
