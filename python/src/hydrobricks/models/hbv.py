from __future__ import annotations

import logging
from abc import abstractmethod
from typing import Any

from hydrobricks._exceptions import ConfigurationError, ModelError
from hydrobricks.models.model import Model

logger = logging.getLogger(__name__)


class HBV(Model):
    """Base class for the HBV model family (Bergström, 1976, and successors).

    This class holds the structure elements shared by the HBV versions: the snow
    routine (degree-day melt with liquid water retention and refreezing), the soil
    moisture routine (beta-function recharge, LP-limited evapotranspiration) and
    the transformation function (triangular unit hydrograph, MAXBAS). The response
    routine differs between versions and must be provided by the subclasses through
    ``_define_response_structure()`` (e.g. the non-linear upper zone of HBV-96).

    The model is integrated by the ODE solver (as Socont), so the results are a
    continuous approximation of the original discrete HBV formulations.

    Options
    -------
    snow_melt_process : str
        Snowmelt method (default: 'melt:degree_day'). Must be 'melt:degree_day'
        when snow refreezing is enabled.
    snow_water_retention_process : str or None
        Outflow process of the snowpack liquid water storage (default:
        'outflow:snow_holding', the HBV holding capacity CWH). None disables the
        liquid water retention (melt water leaves the snowpack directly).
    snow_refreezing_process : str or None
        Refreezing process of the retained liquid water (default:
        'refreeze:degree_day', the HBV refreezing coefficient CFR). None disables
        refreezing. Requires a snow water retention process.
    rain_to_snowpack : bool
        Route the rain to the snowpack liquid water storage instead of the ground
        (default: True, as in the original HBV snow routine). The rain is retained
        in the snowpack (up to the holding capacity CWH) and exposed to
        refreezing; without snow, it reaches the ground within the same time
        step. Requires a snow water retention process.
    snow_rain_process : str or None
        Rain/snow partitioning method (default: None, i.e. 'snow_rain:linear',
        which matches the HBV-96 linear transition over TT ± TTI/2).
    snow_redistribution : str or None
        Optional snow redistribution process (e.g. 'transport:snow_slide').
    """

    @abstractmethod
    def __init__(self, name: str = "hbv", **kwargs: Any) -> None:
        super().__init__(name=name, **kwargs)

        # Default options
        self.options["snow_melt_process"] = "melt:degree_day"
        self.options["snow_water_retention_process"] = "outflow:snow_holding"
        self.options["snow_refreezing_process"] = "refreeze:degree_day"
        self.options["rain_to_snowpack"] = True
        self.options["snow_rain_process"] = None
        self.options["snow_redistribution"] = None
        self.allowed_land_cover_types = ["ground"]

        self._set_options(kwargs)

        try:
            self._define_structure()
            self._generate_structure()
            self._define_parameter_aliases()
            self._define_parameter_constraints()
            self._define_parameter_transforms()

        except RuntimeError as err:
            raise ModelError(
                f"{type(self).__name__} model initialization raised "
                f"an exception: {err}"
            )

    def _define_structure(self) -> None:
        """Define the structure elements shared by the HBV versions.

        - Ground land cover: splits the incoming water (rain + snowpack outflow)
          between the soil moisture storage and the response routine using the
          HBV beta function (recharge = in × (SM/FC)^beta).
        - Response routine: defined by the subclass (``_define_response_structure``).
          Its bricks must route their outflows to the 'routing' brick.
        - Soil moisture storage (capacity FC): evapotranspiration limited by LP;
          overflow safety to the routing brick.
        - Routing: triangular unit hydrograph (MAXBAS) to the outlet.

        The brick declaration order matters: the solver applies the bricks in
        order, so every brick-to-brick flux must flow towards a later brick to be
        received within the same iteration (hence the soil moisture is declared
        after the response routine, which may feed it through a capillary flux).
        """
        # Beta-function split of rain and snowpack outflow. The recharge
        # (outflow:rest) is the complement of the infiltration and must be
        # declared after it.
        self.structure["ground"] = {
            "attach_to": "hydro_unit",
            "kind": "land_cover",
            "processes": {
                "infiltration": {
                    "kind": "infiltration:hbv",
                    "target": "soil_moisture",
                },
                "recharge": {"kind": "outflow:rest", "target": "upper_zone"},
            },
        }

        # Response routine (version-specific)
        self._define_response_structure()

        # Soil moisture storage (capacity FC). The overflow is a numerical safety
        # only (the infiltration and the capillary flux both vanish at FC).
        self.structure["soil_moisture"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "parameters": {"capacity": 250},
            "processes": {
                "et": {"kind": "et:hbv"},
                "overflow": {"kind": "overflow", "target": "routing"},
            },
        }

        # Transformation function (triangular unit hydrograph)
        self.structure["routing"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {
                "routing": {"kind": "routing:hbv", "target": "outlet"},
            },
        }

    @abstractmethod
    def _define_response_structure(self) -> None:
        """Define the version-specific response routine bricks.

        The bricks must receive the recharge from the 'upper_zone' target and
        route their outflows to the 'routing' brick.
        """
        raise ConfigurationError(
            f"The response routine has to be defined by the child class "
            f"(named {self.name}).",
            reason="Abstract method not implemented",
        )

    def _define_parameter_aliases(self) -> None:
        """Define common HBV parameter aliases (literature names).

        - fc: Soil moisture storage capacity (FC).
        - cfmax: Snow melt degree-day factor (CFMAX).
        - tt: Snow melt threshold temperature (TT).

        The process parameter specs already provide lp, beta, maxbas, cfr (snow
        refreezing coefficient) and cwh/whc (snowpack water holding capacity).
        """
        self.parameter_aliases = {
            "soil_moisture:capacity": ["fc"],
            "type:snowpack:degree_day_factor": ["cfmax"],
            "type:snowpack:melting_temperature": ["tt"],
        }

    def _define_parameter_constraints(self) -> None:
        """Define parameter constraints (none required for HBV)."""
        self.parameter_constraints = []

    def _set_specific_options(self, kwargs: dict[str, Any]) -> None:
        """Set and validate HBV-specific configuration options."""
        retention = self.options.get("snow_water_retention_process")
        refreezing = self.options.get("snow_refreezing_process")

        if self.options.get("rain_to_snowpack") and retention is None:
            raise ConfigurationError(
                "Routing the rain to the snowpacks requires a snow water "
                "retention process.",
                item_name="rain_to_snowpack",
                item_value=True,
                reason="Missing snow water retention process",
            )

        if refreezing is not None:
            if retention is None:
                raise ConfigurationError(
                    "Snow refreezing requires a snow water retention process.",
                    item_name="snow_refreezing_process",
                    item_value=refreezing,
                    reason="Missing snow water retention process",
                )
            if self.options.get("snow_melt_process") != "melt:degree_day":
                raise ConfigurationError(
                    "The refreeze:degree_day process requires the melt:degree_day "
                    "snow melt process.",
                    item_name="snow_melt_process",
                    item_value=self.options.get("snow_melt_process"),
                    reason="Incompatible option",
                )
