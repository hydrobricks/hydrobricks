"""Model-evaluation data: the observed signals a model is compared against.

This subpackage holds the reference series used to evaluate and calibrate a model
(as opposed to the meteorological *forcing*, which is model input and lives in
``forcing.py``):

- :class:`DischargeObservations` — the primary signal (observed discharge).
- :class:`AuxiliaryObservation` — base class for additional signals.
- :class:`GlacierMassBalanceObservations` — observed glacier mass balance.
- :class:`SnowCoverObservations` — observed snow cover fraction (e.g. MODIS).

It also exposes :func:`evaluate`, the HydroErr-based goodness-of-fit helper.
"""

from hydrobricks.evaluation.base import AuxiliaryObservation, RecordingRequest
from hydrobricks.evaluation.discharge import DischargeObservations
from hydrobricks.evaluation.glacier_mass_balance import GlacierMassBalanceObservations
from hydrobricks.evaluation.metrics import evaluate
from hydrobricks.evaluation.snow_cover import SnowCoverObservations

__all__ = (
    "evaluate",
    "DischargeObservations",
    "AuxiliaryObservation",
    "RecordingRequest",
    "GlacierMassBalanceObservations",
    "SnowCoverObservations",
)
