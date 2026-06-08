from __future__ import annotations

import logging
import math
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
    discrete : bool
        Build method for the production store and routing. True (default) computes
        them directly, reproducing the exact discrete GR4J equations. False
        integrates them with the ODE solver; provided for comparison, it differs
        from the reference GR4J (continuous integration is not the exact discrete map).
    snow_melt_process : str or None
        Snowmelt method: None (no snow), 'melt:cemaneige', or 'melt:degree_day'.
    snow_redistribution : str or None
        Optional snow redistribution process (e.g. 'transport:snow_slide').
    """

    def __init__(self, name: str = "gr4j", **kwargs: Any) -> None:
        super().__init__(name=name, **kwargs)

        # Default options
        self.options["discrete"] = True
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
            self._define_parameter_transforms()

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
        """Define the GR4J model structure with all bricks and processes.

        Two equivalent formulations are available via the 'discrete' option:
          - discrete=True (default): the production store and routing are computed
            directly (no ODE solver), reproducing the exact discrete GR4J equations.
          - discrete=False: the production store and routing are integrated by the
            ODE solver. Provided for comparison; it differs from the reference GR4J
            because continuous integration is not the exact discrete map.
        """
        discrete = self.options["discrete"]

        # ------------------------------------------------------------------ #
        # Ground land cover
        # ------------------------------------------------------------------ #
        # P–E neutralization: interception sends min(P, E) to atmosphere. The net
        # precipitation Pn is passed instantaneously onward (to the production store
        # in the discrete formulation, or to ground_soil in the solver-based one).
        # This mirrors the GR4J paper: "an interception storage of zero capacity".
        self.structure["ground"] = {
            "attach_to": "hydro_unit",
            "kind": "land_cover",
            "processes": {
                "interception": {
                    "kind": "interception:gr4j",
                },
                "throughfall": {
                    "kind": "outflow:rest",
                    "target": "production_store" if discrete else "ground_soil",
                    "instantaneous": True,
                },
            },
        }

        if discrete:
            self._define_structure_discrete()
        else:
            self._define_structure_solver()

    def _define_structure_discrete(self) -> None:
        """Discrete (exact) GR4J: production store and routing computed directly."""
        # ------------------------------------------------------------------ #
        # Production store (capacity X1)
        # ------------------------------------------------------------------ #
        # A single discrete process applies the exact per-step update
        # (infiltration Ps, percolation Perc) and routes PR = (Pn − Ps) + Perc to
        # the unit hydrograph input, while the et process draws the evaporation Es
        # to the atmosphere. The brick is computed directly (no ODE solver).
        self.structure["production_store"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "parameters": {"capacity": 350},
            "computed_directly": True,
            "processes": {
                "production": {
                    "kind": "production:gr4j",
                    "target": "uh_input",
                },
                "et": {
                    "kind": "et:gr4j",
                },
            },
        }

        # ------------------------------------------------------------------ #
        # UH input buffer (accumulates PR = Pn − Ps + Perc each timestep)
        # ------------------------------------------------------------------ #
        self.structure["uh_input"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "computed_directly": True,
            "processes": {
                "routing": {
                    "kind": "routing:gr4j",
                    "target": "outlet",
                },
            },
        }

    def _define_structure_solver(self) -> None:
        """Solver-based GR4J (for comparison): bricks integrated by the ODE solver."""
        # ------------------------------------------------------------------ #
        # Ground soil (zero-capacity pass-through)
        # ------------------------------------------------------------------ #
        # Splits Pn into infiltration (Ps → production store) and the direct routing
        # branch (Pn − Ps → UH input). runoff:outflow:rest is the complement of
        # infiltration and must be declared after it.
        self.structure["ground_soil"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {
                "production": {
                    "kind": "infiltration:gr4j",
                    "target": "production_store",
                },
                "runoff": {
                    "kind": "outflow:rest",
                    "target": "uh_input",
                },
            },
        }

        # ------------------------------------------------------------------ #
        # Production store (capacity X1): percolation and ET integrated by the solver
        # ------------------------------------------------------------------ #
        self.structure["production_store"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "parameters": {"capacity": 350},
            "processes": {
                "percolation": {
                    "kind": "percolation:gr4j",
                    "target": "uh_input",
                },
                "et": {
                    "kind": "et:gr4j",
                },
            },
        }

        # ------------------------------------------------------------------ #
        # UH input buffer
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
            "production_store:capacity": ["X1", "x1"]
            # Others were already defined in parameter specs.
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

    def _define_parameter_transforms(self) -> None:
        """Define real <-> transformed parameter mappings for the GR4J model.

        Some parameters calibrate better when the optimizer searches a transformed
        space while the model keeps the real value. Each entry maps a parameter
        (by ``component:name`` or alias) to a ``(to_transformed, to_real)`` pair of
        monotonic callables. The real value is always what is sent to the C++ engine.

        X1, X2 and X3 follow the airGR ``TransfoParam_GR4J`` transforms (Coron et
        al., 2017): X1/X3 in log space, X2 via the inverse hyperbolic sine (it spans
        negative to positive exchange). X4 uses the original GR4J spreadsheet
        transform, real = exp(t) + 0.5 (inverse log(X4 - 0.5)), which is log-like and
        enforces the physical floor X4 > 0.5.

        Both X1 and X4 use a log that is undefined at their default lower bounds
        (X1 at 0, X4 at 0.5), so those lower bounds are raised via ``parameter_ranges``
        (X4 to 0.51, the minimum useful unit-hydrograph length). Only the lower bound
        is overridden; the upper bound keeps flowing from the parameter specs.
        """
        self.parameter_ranges = {
            # X1's log transform needs X1 > 0; raise the lower bound (spec min is 0).
            "production_store:capacity": (1.0, None),
            # X4's log(X4 - 0.5) transform needs X4 > 0.5; raise the lower bound to
            # the minimum useful UH length (spec min is 0.5).
            "uh_input:uh_base_time": (0.51, None),
        }
        self.parameter_transforms = {
            # X1 (production store capacity): log space (X1 > 0, see parameter_ranges).
            "production_store:capacity": (math.log, math.exp),
            # X2 (groundwater exchange): inverse hyperbolic sine (handles sign).
            "uh_input:exchange_factor": (math.asinh, math.sinh),
            # X3 (routing store capacity): log space (X3 > 0).
            "uh_input:routing_capacity": (math.log, math.exp),
            # X4 (unit hydrograph time base): original GR4J spreadsheet transform,
            # real = exp(t) + 0.5 (inverse log(X4 - 0.5)); needs X4 > 0.5.
            "uh_input:uh_base_time": (
                lambda x4: math.log(x4 - 0.5),  # real -> transformed
                lambda t: math.exp(t) + 0.5,  # transformed -> real
            ),
        }

    def _set_specific_options(self, kwargs: dict[str, Any]) -> None:
        """Set GR4J-specific configuration options."""
        if "discrete" in kwargs:
            val = kwargs["discrete"]
            if not isinstance(val, bool):
                raise ConfigurationError(
                    f"The option 'discrete' must be a boolean (got '{val}').",
                    item_name="discrete",
                    item_value=val,
                    reason="Invalid option value",
                )
            self.options["discrete"] = val

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
