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

    Precipitation undercatch can be corrected with the rainfall and snowfall
    correction factors ``rfcf`` and ``sfcf`` (both default 1.0), applied to the
    rain and snow components at the snow/rain splitter.

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
    share_soil : bool
        Share a single soil moisture storage across all land covers (default:
        False, i.e. each land cover has its own soil moisture store, as in the
        original HBV land-use formulation). With several land covers and per-class
        soils, the soil/recharge parameters (fc, lp, beta) become cover-specific
        and are exposed with a per-cover suffix (e.g. ``fc_forest``); with a single
        land cover (or when sharing) the bare aliases (``fc``, ``lp``, ``beta``)
        are kept.
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
        self.options["share_soil"] = False
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

        - Land covers: each splits the incoming water (rain + snowpack outflow)
          between its soil moisture storage and the response routine using the HBV
          beta function (recharge = in × (SM/FC)^beta).
        - Response routine: defined by the subclass (``_define_response_structure``).
          Its bricks must route their outflows to the 'routing' brick.
        - Soil moisture storage (capacity FC): evapotranspiration limited by LP;
          overflow safety to the routing brick. One per land cover (original HBV
          land-use formulation), or a single shared store when ``share_soil`` is
          set.
        - Routing: triangular unit hydrograph (MAXBAS) to the outlet.

        The brick declaration order matters: the solver applies the bricks in
        order, so every brick-to-brick flux must flow towards a later brick to be
        received within the same iteration (hence the soil moisture stores are
        declared after the response routine, which may feed them through a
        capillary flux, and the routing is declared last).
        """
        # Soil naming: a single shared store when requested or with one land cover,
        # otherwise one soil moisture store per land cover.
        self._shared_soil = (
            bool(self.options.get("share_soil")) or len(self.land_cover_names) == 1
        )
        if self._shared_soil:
            self._soil_names = {name: "soil_moisture" for name in self.land_cover_names}
        else:
            self._soil_names = {
                name: f"{name}_soil_moisture" for name in self.land_cover_names
            }
        multi_cover = len(self.land_cover_names) > 1

        # Beta-function split of rain and snowpack outflow per land cover. The
        # recharge (outflow:rest) is the complement of the infiltration and must be
        # declared after it.
        for cover_name in self.land_cover_names:
            brick = {
                "attach_to": "hydro_unit",
                "kind": "land_cover",
                "processes": {
                    "infiltration": {
                        "kind": "infiltration:hbv",
                        "target": self._soil_names[cover_name],
                    },
                    "recharge": {"kind": "outflow:rest", "target": "upper_zone"},
                },
            }
            if multi_cover:
                brick["alias_suffix"] = f"_{cover_name}"
            self.structure[cover_name] = brick

        # Response routine (version-specific)
        self._define_response_structure()

        # Soil moisture storage(s) (capacity FC). The overflow is a numerical safety
        # only (the infiltration and the capillary flux both vanish at FC).
        for cover_name, soil_name in self._soil_names.items():
            if soil_name in self.structure:
                continue  # shared store already declared
            soil = {
                "attach_to": "hydro_unit",
                "kind": "storage",
                "parameters": {"capacity": 250},
                "processes": {
                    "et": {"kind": "et:hbv"},
                    "overflow": {"kind": "overflow", "target": "routing"},
                },
            }
            if not self._shared_soil and multi_cover:
                soil["alias_suffix"] = f"_{cover_name}"
            self.structure[soil_name] = soil

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

        - fc: Soil moisture storage capacity (FC). With per-class soils, one alias
          per cover (e.g. ``fc_forest``); shared/single soil keeps the bare ``fc``.
        - cfmax: Snow melt degree-day factor (CFMAX).
        - tt: Snow melt threshold temperature (TT).

        The process parameter specs already provide lp, beta, maxbas, cfr (snow
        refreezing coefficient) and cwh/whc (snowpack water holding capacity); with
        per-class soils these likewise carry a per-cover suffix (lp_<cover>,
        beta_<cover>).
        """
        self.parameter_aliases = {
            "type:snowpack:degree_day_factor": ["cfmax"],
            "type:snowpack:melting_temperature": ["tt"],
        }
        if self._shared_soil:
            self.parameter_aliases["soil_moisture:capacity"] = ["fc"]
        else:
            for cover_name, soil_name in self._soil_names.items():
                self.parameter_aliases[f"{soil_name}:capacity"] = [f"fc_{cover_name}"]

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
