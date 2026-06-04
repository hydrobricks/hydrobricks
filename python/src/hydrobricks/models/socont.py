from __future__ import annotations

import logging
import math
from typing import Any

from hydrobricks._exceptions import ConfigurationError, ModelError
from hydrobricks.models import Model

logger = logging.getLogger(__name__)


class Socont(Model):
    """Socont model implementation"""

    def __init__(self, name: str = "socont", **kwargs: Any) -> None:
        super().__init__(name=name, **kwargs)

        # Default options
        self.options["soil_storage_nb"] = 1
        self.options["surface_runoff"] = "socont_runoff"
        self.options["snow_melt_process"] = "melt:degree_day"
        self.options["snow_ice_transformation"] = None
        self.options["snow_redistribution"] = None
        self.options["glacier_infinite_storage"] = True
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
        # Add surface-related processes
        for cover_type, cover_name in zip(self.land_cover_types, self.land_cover_names):
            if cover_type == "glacier":
                self.structure[cover_name] = {
                    "attach_to": "hydro_unit",
                    "kind": "land_cover",
                    "parameters": {
                        "no_melt_when_snow_cover": True,
                        "infinite_storage": self.options["glacier_infinite_storage"],
                    },
                    "processes": {
                        "outflow_rain_snowmelt": {
                            "kind": "outflow:direct",
                            "target": "glacier_area_rain_snowmelt_storage",
                            "instantaneous": True,
                        },
                        "melt": {
                            "kind": self.options["snow_melt_process"],
                            "target": "glacier_area_icemelt_storage",
                            "instantaneous": True,
                        },
                    },
                }

        if "glacier" in self.land_cover_types:
            # Basin storages for contributions from the glacierized area
            self.structure["glacier_area_rain_snowmelt_storage"] = {
                "attach_to": "sub_basin",
                "kind": "storage",
                "processes": {
                    "outflow": {"kind": "outflow:linear", "target": "outlet"}
                },
            }
            self.structure["glacier_area_icemelt_storage"] = {
                "attach_to": "sub_basin",
                "kind": "storage",
                "processes": {
                    "outflow": {"kind": "outflow:linear", "target": "outlet"}
                },
            }

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
            "glacier_area_rain_snowmelt_storage:response_factor": "k_snow",
            "glacier_area_icemelt_storage:response_factor": "k_ice",
            "surface_runoff:response_factor": "k_quick",
        }

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
