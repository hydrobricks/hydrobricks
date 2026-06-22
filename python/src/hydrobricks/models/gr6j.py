from __future__ import annotations

import logging
import math
from typing import Any

from hydrobricks._exceptions import ModelError
from hydrobricks.models.gr4j import GR4J
from hydrobricks.models.model import Model

logger = logging.getLogger(__name__)


class GR6J(GR4J):
    """GR6J daily rainfall-runoff model (Pushpalatha et al., 2011).

    GR6J extends GR4J with two changes aimed at improving low-flow simulation,
    while keeping the production store, interception, throughfall and ET identical
    to GR4J:

      - a threshold-based groundwater exchange ``F = X2 (R/X3 - X5)`` (the GR5J
        formulation), and
      - an additional exponential routing store (coefficient X6) placed in
        parallel to the power routing store. The UH1 output is split 60% to the
        power store and 40% to the exponential store.

    Parameters
    ----------
    X1 : float
        Production store capacity [mm].
    X2 : float
        Groundwater exchange coefficient [mm/d]. Negative = loss, positive = gain.
    X3 : float
        Routing store maximum capacity [mm].
    X4 : float
        Unit hydrograph time base [d]. Must be > 0.5.
    X5 : float
        Groundwater exchange threshold [-]. Sets the routing-store filling ratio at
        which the exchange changes sign.
    X6 : float
        Exponential store coefficient [mm]. Must be > 0.

    Options
    -------
    discrete : bool
        Build method for the production store and routing. True (default) computes
        them directly, reproducing the exact discrete equations. False integrates
        them with the ODE solver (provided for comparison).
    snow_melt_process : str or None
        Snowmelt method: None (no snow), 'melt:degree_day',
        'melt:degree_day_aspect', 'melt:temperature_index', or 'melt:cemaneige'.
        'melt:temperature_index' requires a 'solar_radiation' forcing, and
        'melt:degree_day_aspect' an 'aspect_class' hydro unit property.
    snow_redistribution : str or None
        Optional snow redistribution process (e.g. 'transport:snow_slide').
    """

    def __init__(self, name: str = "gr6j", **kwargs: Any) -> None:
        # Mirror GR4J.__init__ so the error message and option defaults are correct
        # for GR6J; the structure/alias/transform hooks below are overridden.
        Model.__init__(self, name=name, **kwargs)

        # Default options
        self.options["discrete"] = True
        self.options["snow_melt_process"] = None
        self.options["snow_rain_process"] = None
        self.options["snow_redistribution"] = None
        self.allowed_land_cover_types = ["open"]

        self._set_options(kwargs)

        try:
            self._define_structure()
            self._generate_structure()
            self._define_parameter_aliases()
            self._define_parameter_constraints()
            self._define_parameter_transforms()

        except RuntimeError as err:
            raise ModelError(f"GR6J model initialization raised an exception: {err}")

    def _define_structure_discrete(self) -> None:
        """Discrete (exact) GR6J: same as GR4J but routed by GR6J routing."""
        super()._define_structure_discrete()
        self.structure["uh_input"]["processes"]["routing"]["kind"] = "routing:gr6j"

    def _define_structure_solver(self) -> None:
        """Solver-based GR6J (for comparison): same as GR4J but with GR6J routing."""
        super()._define_structure_solver()
        self.structure["uh_input"]["processes"]["routing"]["kind"] = "routing:gr6j"

    def _define_parameter_aliases(self) -> None:
        """Define user-friendly parameter aliases for the GR6J model (X1-X6)."""
        super()._define_parameter_aliases()

    def _define_parameter_transforms(self) -> None:
        """Define real <-> transformed parameter mappings for the GR6J model.

        Inherits the GR4J transforms (X1-X4) and adds:
          - X5 (exchange threshold): inverse hyperbolic sine (spans negative to
            positive, like X2).
          - X6 (exponential store coefficient): log space (X6 > 0).
        """
        super()._define_parameter_transforms()
        self.parameter_transforms.update(
            {
                "uh_input:exchange_threshold": (math.asinh, math.sinh),
                "uh_input:exp_store_coeff": (math.log, math.exp),
            }
        )
