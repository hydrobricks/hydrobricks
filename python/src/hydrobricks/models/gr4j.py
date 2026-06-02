from __future__ import annotations

import logging
from typing import Any

from hydrobricks._exceptions import ConfigurationError, ModelError
from hydrobricks.models.model import Model

logger = logging.getLogger(__name__)


class GR4J(Model):
    """GR4J daily rainfall-runoff model (Perrin et al., 2003).

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

    Options
    -------
    snow_melt_process : str or None
        Snowmelt method: None (no snow), 'melt:cemaneige', or 'melt:degree_day'.
    snow_redistribution : str or None
        Optional snow redistribution process (e.g. 'transport:snow_slide').
    """

    def __init__(self, name: str = "gr4j", **kwargs: Any) -> None:
        super().__init__(name=name, **kwargs)

        # Default options
        self.options["snow_melt_process"] = None
        self.options["snow_rain_process"] = None
        self.options["snow_redistribution"] = None
        self.allowed_land_cover_types = ["ground"]

        self._set_options(kwargs)

        try:
            self._define_structure()
            self._generate_structure()
            self._define_parameter_aliases()
            self._define_parameter_constraints()

        except RuntimeError as err:
            raise ModelError(f"GR4J model initialization raised an exception: {err}")

    def _set_structure_basics(self) -> None:
        snow_melt_process = self.options.get("snow_melt_process")
        with_snow = snow_melt_process is not None
        snow_rain_process = self.options.get("snow_rain_process")
        snow_redistribution = self.options.get("snow_redistribution")

        self.settings.generate_base_structure(
            self.land_cover_names,
            self.land_cover_types,
            with_snow=with_snow,
            snow_melt_process=snow_melt_process or "melt:degree_day",
            snow_rain_process=snow_rain_process,
            snow_redistribution=snow_redistribution,
        )

    def _define_structure(self) -> None:
        """Define the GR4J model structure with all bricks and processes."""
        # ------------------------------------------------------------------ #
        # Ground land cover
        # ------------------------------------------------------------------ #
        # P–E neutralization: sends min(P, E) to atmosphere.  The remainder
        # (Pn) is passed instantaneously to ground_soil so that the solver
        # for that brick always sees net precipitation, not raw P.
        # This mirrors the GR4J paper: "an interception storage of zero capacity".
        self.structure["ground"] = {
            "attach_to": "hydro_unit",
            "kind": "land_cover",
            "processes": {
                "interception": {
                    "kind": "interception:gr4j",
                },
                "throughfall": {
                    "kind": "outflow:rest_direct",
                    "target": "ground_soil",
                    "instantaneous": True,
                },
            },
        }

        # Zero-capacity pass-through: receives Pn from ground, splits into
        # production-store infiltration (Ps) and direct routing (Pn − Ps).
        self.structure["ground_soil"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {
                "production": {
                    "kind": "infiltration:gr4j",
                    "target": "production_store",
                },
                "runoff": {
                    "kind": "outflow:rest_direct",
                    "target": "uh_input",
                },
            },
        }

        # ------------------------------------------------------------------ #
        # Production store (capacity X1)
        # ------------------------------------------------------------------ #
        self.structure["production_store"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "parameters": {"capacity": 350},
            "processes": {
                "et": {
                    "kind": "et:gr4j",
                },
                "percolation": {
                    "kind": "percolation:gr4j",
                    "target": "uh_input",
                },
            },
        }

        # ------------------------------------------------------------------ #
        # UH input buffer (accumulates PR = Pn − Ps + Perc each timestep)
        # ------------------------------------------------------------------ #
        self.structure["uh_input"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {
                "routing": {
                    "kind": "routing:gr4j",
                    "target": "outlet",
                },
            },
        }

    def _define_parameter_aliases(self) -> None:
        """Define user-friendly parameter aliases for the GR4J model."""
        self.parameter_aliases = {
            "production_store:capacity": ["X1", "x1"],
            "uh_input:exchange_factor": ["X2", "x2"],
            "uh_input:routing_capacity": ["X3", "x3"],
            "uh_input:uh_base_time": ["X4", "x4"],
        }

        if self.options["snow_melt_process"] == "melt:cemaneige":
            self.parameter_aliases.update(
                {
                    "ground_snowpack:degree_day_factor": ["Kf", "kf"],
                    "ground_snowpack:cold_content_factor": ["CTG", "ctg"],
                    "ground_snowpack:melting_temperature": ["Tmelt", "tmelt"],
                    "ground_snowpack:mean_annual_snow": ["Cn", "cn"],
                }
            )
        elif self.options["snow_melt_process"] == "melt:degree_day":
            self.parameter_aliases.update(
                {
                    "ground_snowpack:degree_day_factor": ["a_snow", "kf"],
                    "ground_snowpack:melting_temperature": ["Tmelt", "tmelt"],
                }
            )

    def _define_parameter_constraints(self) -> None:
        """Define parameter constraints for the GR4J model."""
        # X4 must be positive (enforced by the routing process)
        self.parameter_constraints = []

    def _set_specific_options(self, kwargs: dict[str, Any]) -> None:
        """Set GR4J-specific configuration options."""
        if "snow_melt_process" in kwargs:
            allowed = (None, "melt:cemaneige", "melt:degree_day")
            val = kwargs["snow_melt_process"]
            if val not in allowed:
                raise ConfigurationError(
                    f"The option 'snow_melt_process' value '{val}' "
                    f"is not recognised in GR4J. Allowed values: {allowed}",
                    item_name="snow_melt_process",
                    item_value=val,
                    reason="Invalid option value",
                )
            self.options["snow_melt_process"] = val
