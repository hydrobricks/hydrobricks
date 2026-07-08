from __future__ import annotations

import logging
from abc import abstractmethod
from typing import Any

from hydrobricks._exceptions import ConfigurationError, ModelError
from hydrobricks.models.model import Model
from hydrobricks.models.model_settings import WATER_COVER_TYPE
from hydrobricks.modules.glacier import GlacierModule

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
    snow_sublimation_process : str or None
        Optional snow sublimation process removing snow directly to the atmosphere
        (default: None). One of 'sublimation:constant' or 'sublimation:pet'.
    share_soil : bool
        Share a single soil moisture storage across all land covers (default:
        False, i.e. each land cover has its own soil moisture store, as in the
        original HBV land-use formulation). With several land covers and per-class
        soils, the soil/recharge parameters (fc, lp, beta) become cover-specific
        and are exposed with a per-cover suffix (e.g. ``fc_forest``); with a single
        land cover (or when sharing) the bare aliases (``fc``, ``lp``, ``beta``)
        are kept.
    forest_interception : bool
        Add a canopy interception store on each ``forest`` land cover (default:
        False). When enabled, the canopy intercepts rain (capacity ``ic``),
        evaporates at the potential rate and passes the excess as throughfall; when
        disabled, a ``forest`` cover behaves like a generic soil cover.
    glacier_infinite_storage : bool
        Treat the glacier ice as an infinite storage (default: True), as in Socont.
    glacier_module : str
        Glacier formulation to plug in (default: 'gsm', the Glacier Sub-Model of
        GSM-SOCONT: two linear reservoirs for the glacierized-area rain + snowmelt
        and ice melt).

    Land-use classes
    ----------------
    Besides the default soil-bearing ``open`` cover, HBV supports the HBV land-use
    classes as land covers: ``forest`` (an optional canopy interception on the rain
    path, enabled with ``forest_interception=True``), ``water`` (exclusive open-water
    cover: all precipitation direct, open-water evaporation, linear outflow — its own
    no-snow structure variant) and ``glacier`` (Socont-style: glacier-area rain +
    snowmelt and ice melt feed two linear sub-basin reservoirs draining to the outlet,
    with a glacier-free base variant). The name ``lake`` is reserved for a future
    distinct cover (a regulated lake/reservoir store with its own release rules) and is
    not currently accepted.
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
        self.options["snow_sublimation_process"] = None
        self.options["share_soil"] = False
        self.options["forest_interception"] = False
        self.options["glacier_infinite_storage"] = True
        self.options["glacier_module"] = "gsm"
        self.allowed_land_cover_types = ["open", "forest", "water", "glacier"]

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

        The open-water cover (``water``) has no soil/snow routine: it is
        excluded from the soil routine here and handled as a separate structure
        variant (see ``_define_structure_variants``). Glaciers (Socont-style) drain
        to catchment-level linear reservoirs, bypassing the soil routine, and add a
        with-glacier structure variant on top of a glacier-free base. The water and
        glacier bricks are still added to this (superset) structure so their
        parameters are generated; the variants select the relevant subsets.
        """
        # Separate the cover categories: open water (exclusive), glaciers (Socont-style,
        # drain to sub-basin reservoirs) and the soil-bearing covers (open, forest, ...)
        # that go through the soil/response/routing routine.
        self._water_cover_names = [
            name
            for name, cover_type in zip(self.land_cover_names, self.land_cover_types)
            if cover_type == WATER_COVER_TYPE
        ]
        self._glacier_cover_names = [
            name
            for name, cover_type in zip(self.land_cover_names, self.land_cover_types)
            if cover_type == "glacier"
        ]
        soil_cover_names = [
            name
            for name, cover_type in zip(self.land_cover_names, self.land_cover_types)
            if cover_type not in (WATER_COVER_TYPE, "glacier")
        ]
        if not soil_cover_names:
            raise ConfigurationError(
                "The HBV model requires at least one soil-bearing land cover "
                "(open or forest).",
                item_name="land_cover_types",
                reason="Only water/glacier covers provided",
            )

        # Soil naming: a single shared store when requested or with one soil cover,
        # otherwise one soil moisture store per soil cover.
        self._shared_soil = (
            bool(self.options.get("share_soil")) or len(soil_cover_names) == 1
        )
        if self._shared_soil:
            self._soil_names = {name: "soil_moisture" for name in soil_cover_names}
        else:
            self._soil_names = {
                name: f"{name}_soil_moisture" for name in soil_cover_names
            }
        multi_cover = len(soil_cover_names) > 1

        # Beta-function split of rain and snowpack outflow per soil cover. The
        # recharge (outflow:rest) is the complement of the infiltration and must be
        # declared after it.
        for cover_name in soil_cover_names:
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

        # Glacier covers (Socont-style by default): delegated to the pluggable glacier
        # module. The glacierized area sends its rain + snowmelt and its ice melt to
        # catchment-level reservoirs draining to the outlet, bypassing the soil routine.
        # The snow on the glacier still melts through its (generated) snowpack; ice melt
        # is suppressed while snow covers it.
        self._glacier_module = GlacierModule.get_module(self.options["glacier_module"])
        self._glacier_module.add_bricks(
            self.structure,
            self._glacier_cover_names,
            melt_process=self.options["snow_melt_process"],
            options=self.options,
        )

        # Water covers: an exclusive open-water cover with no soil/snow. All precip
        # enters a water storage directly; it evaporates at the potential rate and
        # drains through a linear outflow to the outlet. The water cover is a
        # pass-through (instantaneous direct outflow) into the storage, mirroring the
        # Socont glacier land-cover → sub-basin-storage pattern. These bricks live in
        # the water structure variant only (see ``_define_structure_variants``).
        for water_name in self._water_cover_names:
            self.structure[water_name] = {
                "attach_to": "hydro_unit",
                "kind": "land_cover",
                "processes": {
                    "outflow": {
                        "kind": "outflow:direct",
                        "target": f"{water_name}_storage",
                        "instantaneous": True,
                    },
                },
            }
            self.structure[f"{water_name}_storage"] = {
                "attach_to": "hydro_unit",
                "kind": "storage",
                "processes": {
                    "et": {"kind": "et:open_water"},
                    "outflow": {"kind": "outflow:linear", "target": "outlet"},
                },
            }

    def _define_structure_variants(
        self,
    ) -> list[tuple]:
        """Build the structure variants for the land-use classes.

        The primary (structure 1) is the glacier- and water-free **base**: the soil
        covers (open, forest) with the soil/response/routing routine, plus the
        catchment-level glacier reservoirs (kept here so the sub-basin, built from
        structure 1, owns them and they are shared by all units). Optional variants:

        - a **with-glacier** variant (when a glacier cover is present) adding the
          glacier land covers on top of the base, used by glacierized units while
          glacier-free units use the base (as in Socont);
        - a **water** variant (when a water cover is present): only the water
          storage, with no snow (all precipitation enters the open water directly).

        Units are auto-assigned (in the C++ core) to the variant matching their
        present covers. With neither glacier nor water there is a single variant. The
        glacier split (and the glacier formulation) is the shared, pluggable glacier
        module; the water variant is HBV-specific.
        """
        water_brick_keys = set(self._water_cover_names) | {
            f"{name}_storage" for name in self._water_cover_names
        }

        # The non-water covers/bricks (soil covers, plus the glacier covers and the
        # glacier reservoirs the module added). The shared glacier helper splits this
        # into a glacier-free base and a with-glacier variant.
        non_water_names = [
            name
            for name, cover_type in zip(self.land_cover_names, self.land_cover_types)
            if cover_type != WATER_COVER_TYPE
        ]
        non_water_types = [
            cover_type
            for cover_type in self.land_cover_types
            if cover_type != WATER_COVER_TYPE
        ]
        non_water_structure = {
            key: brick
            for key, brick in self.structure.items()
            if key not in water_brick_keys
        }
        variants: list[tuple] = self._split_glacier_variants(
            non_water_names, non_water_types, non_water_structure
        )

        # Water variant: only the water bricks, with no snow (all precipitation enters
        # the open water directly).
        if self._water_cover_names:
            water_names = list(self._water_cover_names)
            water_types = [WATER_COVER_TYPE] * len(water_names)
            water_structure = {
                key: brick
                for key, brick in self.structure.items()
                if key in water_brick_keys
            }
            water_options = {
                "with_snow": False,
                "snow_melt_process": None,
                "snow_water_retention_process": None,
                "snow_refreezing_process": None,
                "snow_sublimation_process": None,
                "rain_to_snowpack": False,
            }
            variants.append((water_names, water_types, water_structure, water_options))

        return variants

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

        # Water (open water) linear outflow response factor.
        single_water = len(self._water_cover_names) == 1
        for water_name in self._water_cover_names:
            alias = "k_water" if single_water else f"k_water_{water_name}"
            self.parameter_aliases[f"{water_name}_storage:response_factor"] = [alias]

        # Glacier-area reservoir response factors, from the (pluggable) glacier module.
        if self._glacier_module is not None:
            self.parameter_aliases.update(
                self._glacier_module.parameter_aliases(self._glacier_cover_names)
            )

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
