from __future__ import annotations

import logging
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
                "runoff": {"kind": "outflow:rest_direct", "target": "surface_runoff"},
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
                "kind": "outflow:percolation",
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
