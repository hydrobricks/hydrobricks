from __future__ import annotations

import logging
import math
from typing import Any

from hydrobricks._exceptions import ConfigurationError, ModelError
from hydrobricks.models import Model
from hydrobricks.modules.glacier import get_glacier_module

logger = logging.getLogger(__name__)


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
        must be < k_slow_1 and < k_quick).
    beta : float
        Quick-flow (surface runoff) coefficient [m^(4/3)/s] of the kinematic-wave
        overland flow (surface_runoff='socont_runoff').
    k_quick : float
        Surface runoff response factor [1/d] when a linear storage is used instead
        (surface_runoff='linear_storage'); must be > k_slow_1.
    k_snow : float
        Response factor of the glacierized-area rain + snowmelt storage [1/d].
    k_ice : float
        Response factor of the glacierized-area ice melt storage [1/d].

    Options
    -------
    soil_storage_nb : int
        Number of slow soil reservoirs, 1 (default) or 2. With 2, a constant
        percolation feeds a second linear soil layer.
    surface_runoff : str
        Quick-flow method: 'socont_runoff' (default, kinematic-wave overland flow)
        or 'linear_storage'.
    snow_melt_process : str
        Snowmelt method (default: 'melt:degree_day').
    snow_ice_transformation : str or None
        Snow-to-ice transformation on the glacier (default: None).
    snow_redistribution : str or None
        Optional snow redistribution process (e.g. 'transport:snow_slide').
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
        self.options["snow_melt_process"] = "melt:degree_day"
        self.options["snow_ice_transformation"] = None
        self.options["snow_redistribution"] = None
        self.options["glacier_infinite_storage"] = True
        self.options["glacier_module"] = "gsm"
        self.allowed_land_cover_types = ["ground", "glacier"]

        self._set_options(kwargs)

        try:
            self._define_structure()
            self._generate_structure()
            self._define_parameter_aliases()
            self._define_parameter_constraints()
            self._define_parameter_transforms()

        except RuntimeError as err:
            raise ModelError(f"Socont model initialization raised an exception: {err}")

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
        self._glacier_module = get_glacier_module(self.options["glacier_module"])
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

        # Infiltration and overflow
        self.structure["ground"] = {
            "attach_to": "hydro_unit",
            "kind": "land_cover",
            "processes": {
                "infiltration": {
                    "kind": "infiltration:socont",
                    "target": "slow_reservoir",
                },
                "runoff": {"kind": "outflow:rest", "target": "surface_runoff"},
            },
        }

        # Add other bricks
        self.structure["slow_reservoir"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "parameters": {"capacity": 200},
            "processes": {
                "et": {"kind": "et:socont"},
                "outflow": {"kind": "outflow:linear", "target": "outlet"},
                "overflow": {"kind": "overflow", "target": "outlet"},
            },
        }

        if self.options["soil_storage_nb"] == 2:
            logger.info("Using 2 soil storages.")
            self.structure["slow_reservoir"]["processes"]["percolation"] = {
                "kind": "percolation:constant",
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
            logger.info("Using a linear storage for the quick flow.")
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
        - k_slow_2 < k_slow_1: Second soil layer slower than first layer

        These constraints ensure the model behaves physically.
        """
        self.parameter_constraints = [
            ["a_snow", "<", "a_ice"],
            ["k_slow_1", "<", "k_quick"],
            ["k_slow_2", "<", "k_quick"],
            ["k_slow_2", "<", "k_slow_1"],
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
        self.parameter_transforms = {
            "slow_reservoir:response_factor": (
                lambda rf: math.log(rf / 24.0),  # real [1/d] -> lk = log(k[1/h])
                lambda lk: 24.0 * math.exp(lk),  # lk -> real [1/d]
            ),
            "slow_reservoir_2:response_factor": (
                lambda rf: math.log(rf / 24.0),
                lambda lk: 24.0 * math.exp(lk),
            ),
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
