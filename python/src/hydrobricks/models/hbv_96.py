from __future__ import annotations

import logging
from typing import Any

from hydrobricks.models.hbv import HBV

logger = logging.getLogger(__name__)


class HBV96(HBV):
    """HBV-96 model (Lindström et al., 1997).

    HBV-96 implements the response routine as an upper, non-linear reservoir and
    a lower, linear reservoir:

      - recharge enters the upper zone (UZ),
      - a constant-rate percolation (PERC) feeds the lower zone (LZ),
      - an optional capillary transport (CFLUX) returns water from the upper zone
        to the soil moisture storage,
      - upper zone runoff: Q0 = k_uz × UZ^(1 + alpha),
      - lower zone runoff: Q1 = k_lz × LZ,
      - Q0 + Q1 are routed through the MAXBAS triangular unit hydrograph.

    Parameters (literature names as aliases)
    ----------------------------------------
    tt : float
        Threshold/melting temperature [°C].
    cfmax : float
        Snow melt degree-day factor [mm/d/°C].
    cfr : float
        Refreezing coefficient [-] (typically 0.05).
    cwh : float
        Snowpack liquid water holding capacity, fraction of SWE [-]
        (typically 0.1).
    fc : float
        Soil moisture storage capacity (FC) [mm].
    lp : float
        Soil moisture fraction above which ET reaches the potential rate [-].
    beta : float
        Shape coefficient of the recharge function [-].
    cflux : float
        Maximum capillary flux from the upper zone to the soil moisture [mm/d].
    k_uz : float
        Upper zone response factor [mm^(-alpha)/d].
    alpha : float
        Non-linearity coefficient of the upper zone [-].
    perc : float
        Constant percolation rate to the lower zone [mm/d].
    k_lz : float
        Lower zone (linear) response factor [1/d].
    maxbas : float
        Base of the triangular unit hydrograph [d].
    """

    def __init__(self, name: str = "hbv96", **kwargs: Any) -> None:
        super().__init__(name=name, **kwargs)

    def _define_response_structure(self) -> None:
        """Define the HBV-96 response routine (non-linear UZ, linear LZ)."""
        self.structure["upper_zone"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {
                "capillary": {"kind": "capillary:hbv", "target": "soil_moisture"},
                "percolation": {
                    "kind": "percolation:constant",
                    "target": "lower_zone",
                },
                "runoff": {"kind": "runoff:hbv", "target": "routing"},
            },
        }

        self.structure["lower_zone"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {
                "outflow": {"kind": "outflow:linear", "target": "routing"},
            },
        }

    def _define_parameter_aliases(self) -> None:
        """Define HBV-96 parameter aliases (literature names)."""
        super()._define_parameter_aliases()
        self.parameter_aliases.update(
            {
                "upper_zone:percolation_rate": ["perc", "PERC"],
                "lower_zone:response_factor": ["k_lz", "k4"],
            }
        )

    def _define_parameter_constraints(self) -> None:
        """Define parameter constraints for the HBV-96 model.

        The lower zone must react slower than a linear upper zone would
        (k_lz < k_uz); both are expressed in [1/d] when alpha = 0 and the
        constraint remains a sensible ordering for alpha > 0.
        """
        self.parameter_constraints = [
            ["k_lz", "<", "k_uz"],
        ]
