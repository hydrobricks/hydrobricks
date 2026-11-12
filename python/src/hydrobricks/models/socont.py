from __future__ import annotations

import logging
import math
from typing import Any

from hydrobricks._exceptions import ConfigurationError, ModelError
from hydrobricks.models import Model
from hydrobricks.modules.glacier import GlacierModule

logger = logging.getLogger(__name__)

SOIL_ET_PROCESSES = ("et:socont", "et:hbv", "et:power_law", "et:exponential")
SOIL_INFILTRATION_PROCESSES = ("infiltration:socont", "infiltration:hbv")
SOIL_PERCOLATION_PROCESSES = ("percolation:constant", "percolation:power_law")
#: Options pinned by each preset. 'original' is the published model (Schaefli et
#: al., 2005), whatever the defaults become; 'hydrobricks' keeps the defaults.
PRESETS = {
    "hydrobricks": {},
    "original": {
        "soil_storage_nb": 1,
        "surface_runoff": "socont_runoff",
        "soil_et_process": "et:socont",
        "infiltration_process": "infiltration:socont",
        "snow_melt_process": "melt:degree_day",
        "snow_ice_transformation": None,
        "snow_redistribution": None,
        "snow_sublimation_process": None,
        "glacier_infinite_storage": True,
        "glacier_module": "gsm",
    },
}


class Socont(Model):
    """SOCONT glacio-hydrological model (Schaefli et al., 2005).

    SOCONT (SOil CONTribution) is a conceptual model for high-mountain
    catchments. Each hydro unit splits precipitation into rain and snow; snow and
    glacier ice melt by a degree-day routine. On the ground land cover the
    incoming water is split by the Socont infiltration function between a slow
    (baseflow) reservoir and the quick surface runoff:

      - slow reservoir (capacity A): linear baseflow outflow, LP-free Socont
        evapotranspiration and a capacity overflow to the outlet; an optional
        second soil layer (``soil_storage_nb=2``) is fed by a constant percolation,
      - surface runoff: the infiltration excess, transformed either by a
        kinematic-wave overland flow (``socont_runoff``) or a linear storage.

    On glacierized areas, rain + snowmelt and ice melt are collected in two
    sub-basin linear storages that drain directly to the outlet (the glacier ice
    is treated as an infinite storage by default).

    The model is integrated by the ODE solver, so the results are a continuous
    approximation of the original discrete SOCONT formulation.

    Parameters (literature names as aliases)
    ----------------------------------------
    a_snow : float
        Snow melt degree-day factor [mm/d/°C].
    a_ice : float
        Glacier ice melt degree-day factor [mm/d/°C] (must be > a_snow).
    A : float
        Slow (soil) reservoir storage capacity [mm].
    k_slow (k_slow_1) : float
        Slow reservoir baseflow response factor [1/d].
    k_slow_2 : float
        Second soil-layer response factor [1/d] (only with soil_storage_nb=2;
        must be < k_quick, but is not ordered against k_slow_1).
    beta : float
        Quick-flow (surface runoff) coefficient [m^(4/3)/s] of the kinematic-wave
        overland flow (surface_runoff='socont_runoff'). Calibrated in log space.
    k_quick : float
        Surface runoff response factor [1/d] when a linear storage is used instead
        (surface_runoff='linear_storage'); must be > k_slow_1.
    k_snow : float
        Response factor of the glacierized-area rain + snowmelt storage [1/d].
    k_ice : float
        Response factor of the glacierized-area ice melt storage [1/d].

    Options
    -------
    preset : str
        'hydrobricks' (default) or 'original'. 'original' sets the options below
        to the published model: one slow reservoir, the kinematic-wave quick flow,
        the Socont evapotranspiration and infiltration, the degree-day melt and the
        GSM glacier module, without snow redistribution or sublimation. Options
        given explicitly still apply, with a warning when they depart from the
        original. The rain/snow split is not pinned.
    soil_storage_nb : int
        Number of slow soil reservoirs, 1 (default) or 2. With 2, a percolation
        feeds a second linear soil layer.
    percolation_process : str
        Percolation to the second soil layer (soil_storage_nb=2):
        'percolation:constant' (default, ``percol`` [mm/d] whatever the wetness) or
        'percolation:power_law' (``percol_max * (S/A)^percol_exp``, the recharge
        following the wetness of the first layer).
    surface_runoff : str
        Quick-flow method: 'socont_runoff' (default, kinematic-wave overland flow)
        or 'linear_storage'.
    soil_et_process : str
        Evapotranspiration from the slow reservoir: 'et:hbv' (default, PET above a
        filling ratio ``lp``, linear below), 'et:socont' (the published
        PET * (S/A)^0.5), 'et:power_law' (PET * (S/A)^et_beta) or 'et:exponential'
        (PET * (1 - exp(-et_alpha * S/A))). The published formulation reaches PET
        only with a full store, which the infiltration then keeps from filling: on
        lowland catchments the calibration then hides the surplus water in a sink.
        Use 'et:socont' to reproduce the published model.
    infiltration_process : str
        Infiltration into the slow reservoir: 'infiltration:socont' (default, the
        share 1 - (S/A)^2 of the input) or 'infiltration:hbv' (1 - (S/A)^beta_soil,
        the exponent calibrated; aliased ``beta_soil`` to keep it apart from the
        quick-flow ``beta``).
    snow_melt_process : str
        Snowmelt method (default: 'melt:degree_day').
    snow_ice_transformation : str or None
        Snow-to-ice transformation on the glacier (default: None).
    snow_redistribution : str or None
        Optional snow redistribution process (e.g. 'transport:snow_slide').
    snow_sublimation_process : str or None
        Optional snow sublimation process removing snow directly to the atmosphere
        (default: None). One of 'sublimation:constant' or 'sublimation:pet'.
    glacier_infinite_storage : bool
        Treat the glacier ice as an infinite storage (default: True).
    glacier_module : str
        Glacier formulation to plug in (default: 'gsm', the Glacier Sub-Model of
        GSM-SOCONT: two linear reservoirs for the glacierized-area rain + snowmelt
        and ice melt).
    """

    def __init__(self, name: str = "socont", **kwargs: Any) -> None:
        super().__init__(name=name, **kwargs)

        # Default options
        self.options["soil_storage_nb"] = 1
        self.options["surface_runoff"] = "socont_runoff"
        self.options["soil_et_process"] = "et:hbv"
        self.options["infiltration_process"] = "infiltration:socont"
        self.options["percolation_process"] = "percolation:constant"
        self.options["snow_melt_process"] = "melt:degree_day"
        self.options["snow_rain_process"] = None
        self.options["snow_ice_transformation"] = None
        self.options["snow_redistribution"] = None
        self.options["snow_sublimation_process"] = None
        self.options["glacier_infinite_storage"] = True
        self.options["glacier_module"] = "gsm"
        self.options["preset"] = "hydrobricks"
        self.allowed_land_cover_types = ["open", "glacier"]

        self._apply_preset(kwargs)
        self._set_options(kwargs)

        try:
            self._define_structure()
            self._generate_structure()
            self._define_parameter_aliases()
            self._define_parameter_constraints()
            self._define_parameter_transforms()

        except RuntimeError as err:
            raise ModelError(f"Socont model initialization raised an exception: {err}")

    def _apply_preset(self, kwargs: dict[str, Any]) -> None:
        """Set the options of the preset, before the explicit ones override them."""
        preset = kwargs.get("preset", self.options["preset"])
        if preset not in PRESETS:
            raise ConfigurationError(
                f"Unknown Socont preset: '{preset}'. "
                f"Expected one of {', '.join(PRESETS)}.",
                item_name="preset",
                item_value=preset,
                reason="Unknown preset",
            )
        self.options.update(PRESETS[preset])
        departures = [
            key
            for key, value in PRESETS[preset].items()
            if key in kwargs and kwargs[key] != value
        ]
        if departures:
            logger.warning(
                f"Socont preset '{preset}' with {', '.join(departures)} changed: "
                f"the model departs from the preset."
            )

    def _define_structure(self) -> None:
        """
        Define the Socont model structure with all bricks and processes.

        Sets up the model's hydrological structure including:
        - Glacier processes (if glacier land cover is present)
        - Ground infiltration and surface runoff
        - Soil reservoirs (slow storage, and optionally second soil layer)
        - Surface runoff routing (Socont or linear storage)
        - ET processes

        The structure is stored in self.structure for later generation.

        Raises
        ------
        RuntimeError
            If surface runoff option is not recognized.
        """
        # Glacier-related bricks, delegated to the (pluggable) glacier module.
        self._glacier_module = GlacierModule.get_module(self.options["glacier_module"])
        glacier_names = [
            cover_name
            for cover_type, cover_name in zip(
                self.land_cover_types, self.land_cover_names
            )
            if cover_type == "glacier"
        ]
        self._glacier_module.add_bricks(
            self.structure,
            glacier_names,
            melt_process=self.options["snow_melt_process"],
            options=self.options,
        )

        # Infiltration and overflow on the soil (open areas) land cover. The cover
        # name follows the (single) non-glacier land cover, defaulting to 'open'; an
        # explicit 'ground' is still accepted (kept for backward compatibility).
        soil_cover_name = next(
            (
                name
                for name, cover_type in zip(
                    self.land_cover_names, self.land_cover_types
                )
                if cover_type != "glacier"
            ),
            "open",
        )
        infiltration = self.options["infiltration_process"]
        if infiltration not in SOIL_INFILTRATION_PROCESSES:
            raise ConfigurationError(
                f"Unknown infiltration process: '{infiltration}'. "
                f"Expected one of {', '.join(SOIL_INFILTRATION_PROCESSES)}.",
                item_name="infiltration_process",
                item_value=infiltration,
                reason="Unknown process type",
            )
        soil_et = self.options["soil_et_process"]
        if soil_et not in SOIL_ET_PROCESSES:
            raise ConfigurationError(
                f"Unknown soil ET process: '{soil_et}'. "
                f"Expected one of {', '.join(SOIL_ET_PROCESSES)}.",
                item_name="soil_et_process",
                item_value=soil_et,
                reason="Unknown process type",
            )

        self.structure[soil_cover_name] = {
            "attach_to": "hydro_unit",
            "kind": "land_cover",
            "processes": {
                "infiltration": {
                    "kind": infiltration,
                    "target": "slow_reservoir",
                },
                "runoff": {"kind": "outflow:rest", "target": "surface_runoff"},
            },
        }
        if infiltration == "infiltration:hbv":
            # Its exponent shares the HBV alias 'beta' with the quick flow.
            self.structure[soil_cover_name]["alias_suffix"] = "_soil"

        # Add other bricks
        self.structure["slow_reservoir"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "parameters": {"capacity": 200},
            "processes": {
                "et": {"kind": soil_et},
                "outflow": {"kind": "outflow:linear", "target": "outlet"},
                "overflow": {"kind": "overflow", "target": "outlet"},
            },
        }

        if self.options["soil_storage_nb"] == 2:
            logger.debug("Using 2 soil storages.")
            percolation = self.options["percolation_process"]
            if percolation not in SOIL_PERCOLATION_PROCESSES:
                raise ConfigurationError(
                    f"Unknown percolation process: '{percolation}'. "
                    f"Expected one of {', '.join(SOIL_PERCOLATION_PROCESSES)}.",
                    item_name="percolation_process",
                    item_value=percolation,
                    reason="Unknown process type",
                )
            self.structure["slow_reservoir"]["processes"]["percolation"] = {
                "kind": percolation,
                "target": "slow_reservoir_2",
            }
            self.structure["slow_reservoir_2"] = {
                "attach_to": "hydro_unit",
                "kind": "storage",
                "processes": {
                    "outflow": {"kind": "outflow:linear", "target": "outlet"}
                },
            }

        # Add surface runoff
        if self.options["surface_runoff"] == "socont_runoff":
            surface_runoff_kind = "runoff:socont"
        elif self.options["surface_runoff"] == "linear_storage":
            logger.debug("Using a linear storage for the quick flow.")
            surface_runoff_kind = "outflow:linear"
        else:
            raise ConfigurationError(
                f"The surface runoff option {self.options['surface_runoff']} "
                f"is not recognised in Socont.",
                item_name="surface_runoff",
                item_value=self.options["surface_runoff"],
                reason="Invalid option",
            )

        self.structure["surface_runoff"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {"runoff": {"kind": surface_runoff_kind, "target": "outlet"}},
        }

    def _define_structure_variants(
        self,
    ) -> list[tuple[list[str], list[str], dict[str, Any]]]:
        """Make the glacier-free structure the base, adding a with-glacier variant.

        When glaciers are present, the primary (base) structure is glacier-free, so
        units with no glacier carry no glacier land-cover brick at all (instead of a
        zero-area one); a with-glacier variant is used by glacier units. The glacier
        reservoirs are catchment-level, so they stay in the base (which builds the
        sub-basin) and are shared by both variants. The split (and the glacier
        formulation) is handled by the shared, pluggable glacier module.
        """
        return self._split_glacier_variants(
            self.land_cover_names, self.land_cover_types, self.structure
        )

    def _define_parameter_aliases(self) -> None:
        """
        Define parameter name aliases for the Socont model.

        Maps component-specific parameter names to user-friendly shorthand names:
        - A: Soil storage capacity
        - k_slow/k_slow_1/k_slow1: First soil layer response factor
        - k_slow_2/k_slow2: Second soil layer response factor
        - k_snow: Snow melt storage response factor
        - k_ice: Glacier ice melt storage response factor
        - k_quick: Surface runoff response factor

        These aliases allow users to specify parameters using simpler names.
        """
        self.parameter_aliases = {
            "slow_reservoir:capacity": "A",
            "slow_reservoir:response_factor": ["k_slow", "k_slow_1", "k_slow1"],
            "slow_reservoir_2:response_factor": ["k_slow_2", "k_slow2"],
            "surface_runoff:response_factor": "k_quick",
        }
        # Glacier reservoir response factors (k_snow, k_ice) come from the glacier
        # module so the formulation stays self-contained and swappable.
        glacier_names = [
            name
            for name, cover_type in zip(self.land_cover_names, self.land_cover_types)
            if cover_type == "glacier"
        ]
        if self._glacier_module is not None:
            self.parameter_aliases.update(
                self._glacier_module.parameter_aliases(glacier_names)
            )

    def _define_parameter_constraints(self) -> None:
        """
        Define parameter constraints for the Socont model.

        Enforces physical relationships between parameters:
        - a_snow < a_ice: Snow melt degree-day < glacier ice melt degree-day
        - k_slow_1 < k_quick: Slow flow slower than quick flow
        - k_slow_2 < k_quick: Second soil layer slower than quick flow

        The two soil layers are not ordered (k_slow_2 < k_slow_1 was dropped): the
        layers are in series with different inputs, so the lower one need not be
        the slower, and in 713 HydroMIP calibrations the constraint truncated the
        optima where the second layer was active (12 % sat on k_slow_2 = k_slow_1)
        and was meaningless where it was not (percol ~ 0 in 41 %). It also
        rejected half of all random parameter sets.
        """
        self.parameter_constraints = [
            ["a_snow", "<", "a_ice"],
            ["k_slow_1", "<", "k_quick"],
            ["k_slow_2", "<", "k_quick"],
        ]

    def _define_parameter_transforms(self) -> None:
        """Define real <-> transformed parameter mappings for the Socont model.

        Use this to express a parameter in its original literature formulation when
        that differs from our internal implementation: the user supplies/optimizes the
        transformed (original-formulation) value, and ``to_real`` converts it to the
        internal value sent to the C++ engine. Each entry maps a parameter (by
        ``component:name`` or alias) to a ``(to_transformed, to_real)`` pair of
        monotonic callables.

        Reference SOCONT (Schaefli et al., 2005; ``socontplusV2_6.m``) calibrates
        the slow-reservoir outflow as ``lk = log(k)`` with ``k`` in ``[1/h]``, since
        its numerical scheme runs in hours (qb = k * s, DT = 24 h). Our internal
        ``response_factor`` is instead a rate in ``[1/d]``; at the daily step the two
        rate constants are related by ``k[1/d] = 24 * k[1/h]``. Optimizing in the
        reference's log space also behaves far better, as the response factor spans
        several orders of magnitude. The transform therefore exposes ``lk`` as the
        transformed (optimizer / literature) value and maps it back to the internal
        ``[1/d]`` rate. The capacity ``A`` [mm] and the quickflow ``beta``
        [m^(4/3)/s] are defined identically to our internals, so they need no
        transform. The second soil layer (``slow_reservoir_2``) is a hydrobricks
        extension with no reference counterpart; the same convention is applied for
        a consistent user-facing parameterization (and is skipped automatically when
        a single soil storage is used).
        """
        self.parameter_ranges = {
            "slow_reservoir:capacity": (10.0, 1000.0),
            "slow_reservoir:lp": (0.1, 1.0),
            "r_snow": (0.001, 0.03),
        }
        self.parameter_defaults = {
            "slow_reservoir:lp": 0.5,
        }
        self.parameter_transforms = {
            "slow_reservoir:response_factor": (
                lambda rf: math.log(rf / 24.0),  # real [1/d] -> lk = log(k[1/h])
                lambda lk: 24.0 * math.exp(lk),  # lk -> real [1/d]
            ),
            "slow_reservoir_2:response_factor": (
                lambda rf: math.log(rf / 24.0),
                lambda lk: 24.0 * math.exp(lk),
            ),
            # The quickflow reservoir's time constant scales with area / beta, so
            # beta acts on a log scale: below ~3000 it shapes the hydrograph, above
            # a threshold that grows with unit area (and flatness) the reservoir
            # empties within the day and beta no longer matters. Searching it
            # linearly over [100, 1e5] would spend nearly all draws on that plateau.
            "surface_runoff:beta": (math.log, math.exp),
        }

    def _set_specific_options(self, kwargs: dict[str, Any]) -> None:
        """
        Set Socont-specific configuration options.

        Processes Socont model options:
        - soil_storage_nb: Number of soil storage layers (1 or 2)

        Parameters
        ----------
        kwargs
            Keyword arguments containing Socont-specific options.

        Raises
        ------
        ValueError
            If soil_storage_nb is not 1 or 2.
        """
        if "soil_storage_nb" in kwargs:
            self.options["soil_storage_nb"] = int(kwargs["soil_storage_nb"])
            if (
                self.options["soil_storage_nb"] < 1
                or self.options["soil_storage_nb"] > 2
            ):
                raise ConfigurationError(
                    'The option "soil_storage_nb" can only be 1 or 2.',
                    item_name="soil_storage_nb",
                    item_value=self.options["soil_storage_nb"],
                    reason="Invalid option value",
                )
