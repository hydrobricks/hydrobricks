from __future__ import annotations

import random
from dataclasses import dataclass
from typing import Callable, Hashable

import pandas as pd

from hydrobricks._exceptions import (
    ConfigurationError,
    DependencyError,
)
from hydrobricks._optional import HAS_SPOTPY, spotpy
from hydrobricks._utils import dump_config_file


@dataclass(frozen=True)
class ParamSpec:
    """Static specification for a parameter."""

    name: str
    unit: str | None = None
    aliases: list[str] | None = None
    min: float | list[float] | None = None
    max: float | list[float] | None = None
    default: float | list[float] | None = None
    mandatory: bool = True

    def to_kwargs(self) -> dict:
        # Return a dict suitable to unpack into define_parameter (excluding component)
        return {
            "name": self.name,
            "unit": self.unit,
            "aliases": None if self.aliases is None else list(self.aliases),
            "min_val": self.min,
            "max_val": self.max,
            "default": self.default,
            "mandatory": self.mandatory,
        }


@dataclass(frozen=True)
class ParameterTransform:
    """Bijective transform between a parameter's real and transformed values.

    The *real* value is what the C++ engine uses (and what is stored in the
    parameter set). The *transformed* value is an alternative representation used
    either for optimisation (e.g. ``log`` of a capacity, so the optimizer searches
    a better-behaved space) or to express a parameter in its original literature
    formulation when that differs from our internal implementation.

    Both functions must be monotonic so that range bounds map consistently.
    """

    to_transformed: Callable[[float], float]  # real -> transformed
    to_real: Callable[[float], float]  # transformed -> real


# -----------------------------------------------------------------------------
# Unified registry for process-like parameter specs.
# Bricks (e.g., reservoirs) keep a separate registry.
# -----------------------------------------------------------------------------

PROCESS_PARAM_SPECS: dict[str, list[ParamSpec]] = {
    # Simple linear reservoir
    "outflow:linear": [
        ParamSpec(
            name="response_factor",
            unit="1/d",
            aliases=[],
            min=0.0001,
            max=1,
            mandatory=True,
        )
    ],
    # Linear reservoir outflow above a storage threshold (PREVAH surface runoff:
    # Q0 = K0 * (SUZ - SGRLUZ)). The response factor carries a distinct name so the
    # process can share a brick with an 'outflow:linear'.
    "outflow:linear_threshold": [
        ParamSpec(
            name="response_factor_threshold",
            unit="1/d",
            aliases=[],
            min=0.0001,
            max=1,
            mandatory=True,
        ),
        ParamSpec(
            name="threshold",
            unit="mm",
            aliases=[],
            min=0,
            max=100,
            default=0.0,
            mandatory=True,
        ),
    ],
    # PREVAH percolation: constant maximum rate gated by the soil moisture state
    # (full rate at saturation, linear ramp between cu*FC and FC, none below cu*FC)
    "percolation:prevah": [
        ParamSpec(
            name="percolation_rate",
            unit="mm/d",
            aliases=["cperc"],
            min=0,
            max=10,
            default=0.1,
            mandatory=True,
        ),
        ParamSpec(
            name="threshold_fraction",
            unit="-",
            aliases=["cu_perc"],
            min=0,
            max=0.9,
            default=0.7,
            mandatory=False,
        ),
    ],
    # PREVAH canopy interception (Menzel asymptotic filling). The interception
    # capacity carries the 'ic' alias like the built-in canopy brick; in custom
    # structures with several canopies, set the brick 'alias_suffix' to keep it
    # globally unique.
    "interception:menzel": [
        ParamSpec(
            name="capacity",
            unit="mm",
            aliases=["ic"],
            min=0,
            max=10,
            default=2.0,
            mandatory=False,
        ),
    ],
    # Fixed-ratio outflow split to two targets (PREVAH SLOWCOMP recharge split)
    "outflow:split": [
        ParamSpec(
            name="split_fraction",
            unit="-",
            aliases=[],
            min=0,
            max=1,
            default=0.8889,
            mandatory=False,
        ),
    ],
    # Socont quick flow
    "runoff:socont": [
        ParamSpec(
            name="beta",
            unit="m^(4/3)/s",
            aliases=["beta"],
            min=100,
            max=30000,
            mandatory=True,
        )
    ],
    # Constant percolation
    "percolation:constant": [
        ParamSpec(
            name="percolation_rate",
            unit="mm/d",
            aliases=["percol"],
            min=0,
            max=10,
            mandatory=True,
        )
    ],
    # Single-threshold snow/rain transition
    "transition:snow_rain:threshold": [
        ParamSpec(
            name="threshold",
            unit="°C",
            aliases=["prec_t"],
            min=-5,
            max=5,
            default=0,
            mandatory=False,
        ),
    ],
    # Snow/rain transition (pseudo-process)
    "transition:snow_rain:linear": [
        ParamSpec(
            name="transition_start",
            unit="°C",
            aliases=["prec_t_start"],
            min=-2,
            max=2,
            default=0,
            mandatory=False,
        ),
        ParamSpec(
            name="transition_end",
            unit="°C",
            aliases=["prec_t_end"],
            min=0,
            max=4,
            default=2,
            mandatory=False,
        ),
    ],
    # Precipitation correction factors (HBV RFCF/SFCF) applied at the snow/rain
    # splitter. Default 1.0 = no correction; optional (not mandatory).
    "correction:snow_rain": [
        ParamSpec(
            name="rain_correction_factor",
            unit="-",
            aliases=["rain_correction_factor", "rain_corr_factor", "rfcf", "rcf"],
            min=0.5,
            max=2.0,
            default=1.0,
            mandatory=False,
        ),
        ParamSpec(
            name="snow_correction_factor",
            unit="-",
            aliases=["snow_correction_factor", "snow_corr_factor", "sfcf", "scf"],
            min=0.5,
            max=2.0,
            default=1.0,
            mandatory=False,
        ),
    ],
    # Rainfall correction factor for no-snow models (rain-only splitter).
    "correction:rain": [
        ParamSpec(
            name="rain_correction_factor",
            unit="-",
            aliases=["rain_correction_factor", "rain_corr_factor", "rfcf", "rcf"],
            min=0.5,
            max=2.0,
            default=1.0,
            mandatory=False,
        ),
    ],
    # Classic degree-day melt (snow + glacier unified specs)
    "melt:degree_day": [
        ParamSpec(
            name="degree_day_factor",
            unit="mm/d/°C",
            aliases=None,
            min=2,
            max=20,
            mandatory=True,
        ),
        ParamSpec(
            name="melting_temperature",
            unit="°C",
            aliases=None,
            min=0,
            max=5,
            default=0,
            mandatory=False,
        ),
    ],
    # Aspect-based degree-day melt
    "melt:degree_day_aspect": [
        ParamSpec(
            name="degree_day_factor_n",
            unit="mm/d/°C",
            aliases=None,
            min=1,
            max=20,
            mandatory=True,  # (snow 0-12, glacier 1-20)
        ),
        ParamSpec(
            name="degree_day_factor_s",
            unit="mm/d/°C",
            aliases=None,
            min=1,
            max=20,
            mandatory=True,  # (snow 2-12, glacier 5-20)
        ),
        ParamSpec(
            name="degree_day_factor_ew",
            unit="mm/d/°C",
            aliases=None,
            min=1,
            max=20,
            mandatory=True,  # (snow 2-12, glacier 5-20)
        ),
        ParamSpec(
            name="melting_temperature",
            unit="°C",
            aliases=None,
            min=0,
            max=5,
            default=0,
            mandatory=False,
        ),
    ],
    # CemaNeige snow melt process
    "melt:cemaneige": [
        ParamSpec(
            name="degree_day_factor",
            unit="mm/d/°C",
            aliases=None,
            min=1,
            max=10,
            mandatory=True,
        ),
        ParamSpec(
            name="cold_content_factor",
            unit="-",
            aliases=None,
            min=0,
            max=1,
            default=0.0,
            mandatory=False,
        ),
        ParamSpec(
            name="melting_temperature",
            unit="°C",
            aliases=None,
            min=0,
            max=5,
            default=0.0,
            mandatory=False,
        ),
        ParamSpec(
            name="mean_annual_snow",
            unit="mm",
            aliases=None,
            min=0,
            max=3000,
            mandatory=True,
        ),
    ],
    # Degree-day melt with a seasonal sine melt factor (PREVAH CRMFMIN/CRMFMAX)
    "melt:degree_day_seasonal": [
        ParamSpec(
            name="degree_day_factor_min",
            unit="mm/d/°C",
            aliases=["crmfmin"],
            min=0.5,
            max=6,
            mandatory=True,
        ),
        ParamSpec(
            name="degree_day_factor_max",
            unit="mm/d/°C",
            aliases=["crmfmax"],
            min=1.5,
            max=12,
            mandatory=True,
        ),
        ParamSpec(
            name="melting_temperature",
            unit="°C",
            aliases=None,
            min=-3,  # PREVAH calibrates a negative snowmelt threshold (T0)
            max=5,
            default=0,
            mandatory=False,
        ),
    ],
    # Temperature-index melt (Hock, 1999)
    "melt:temperature_index": [
        ParamSpec(
            name="melt_factor",
            unit="mm/d/°C",
            aliases=None,
            min=0,
            max=12,
            mandatory=True,
        ),
        ParamSpec(
            name="radiation_coefficient",
            unit="m2/W*mm/d/°C",
            aliases=None,
            min=0,
            max=1,
            mandatory=True,
        ),
        ParamSpec(
            name="melting_temperature",
            unit="°C",
            aliases=None,
            min=0,
            max=5,
            default=0,
            mandatory=False,
        ),
    ],
    # Constant-rate snow sublimation
    "sublimation:constant": [
        ParamSpec(
            name="sublimation_rate",
            unit="mm/d",
            aliases=["sublimation_rate"],
            min=0,
            max=2,
            default=0.1,
            mandatory=False,
        ),
    ],
    # PET-fraction snow sublimation (evapo-sublimation; Herrero & Polo, 2016)
    "sublimation:pet": [
        ParamSpec(
            name="sublimation_pet_factor",
            unit="-",
            aliases=["sublimation_pet_factor", "sublimation_factor"],
            min=0,
            max=1,
            default=0.2,
            mandatory=False,
        ),
    ],
    # Snow/ice constant transformation (dynamic aliases per glacier snowpack)
    "transform:snow_ice_constant": [
        ParamSpec(
            name="snow_ice_transformation_rate",
            unit="mm/d",
            aliases=None,
            min=0,
            max=10,
            default=0.5,
            mandatory=True,
        ),
    ],
    # Snow/ice transformation based on SWAT
    "transform:snow_ice_swat": [
        ParamSpec(
            name="snow_ice_transformation_basal_acc_coeff",
            unit="-",
            aliases=None,
            min=0.001,
            max=0.006,
            default=0.0014,
            mandatory=False,
        ),
        ParamSpec(
            name="north_hemisphere",
            unit="-",
            aliases=None,
            min=0,
            max=1,
            default=1,
            mandatory=False,
        ),
    ],
    # HBV/PREVAH soil moisture recharge split (beta function). PREVAH calibrates
    # values below 1 (CBETA), so the lower bound admits them.
    "infiltration:hbv": [
        ParamSpec(
            name="beta",
            unit="-",
            aliases=["beta"],
            min=0.3,
            max=6,
            default=2.0,
            mandatory=True,
        ),
    ],
    # HBV actual evapotranspiration (limit LP as a fraction of FC)
    "et:hbv": [
        ParamSpec(
            name="lp",
            unit="-",
            aliases=["lp"],
            min=0.3,
            max=1,
            default=0.9,
            mandatory=True,
        ),
        ParamSpec(
            name="et_correction_factor",
            unit="-",
            aliases=["et_correction_factor", "et_corr_factor", "cevpf", "etcf"],
            min=0.5,
            max=2.0,
            default=1.0,
            mandatory=False,
        ),
    ],
    # Power-law actual evapotranspiration (Ea = PET * (S/Smax)^exponent)
    "et:power_law": [
        ParamSpec(
            name="exponent",
            unit="-",
            aliases=["et_beta"],
            min=0.2,
            max=3.0,
            default=0.5,
            mandatory=True,
        ),
    ],
    # Exponential ET (Ea = PET * (1 - exp(-alpha * S/Smax)))
    "et:exponential": [
        ParamSpec(
            name="alpha",
            unit="-",
            aliases=["et_alpha"],
            min=0.5,
            max=10.0,
            default=2.0,
            mandatory=True,
        ),
    ],
    # HBV-96 non-linear upper zone runoff (Q0 = k * UZ^(1+alpha))
    "runoff:hbv": [
        ParamSpec(
            name="response_factor",
            unit="mm^(-alpha)/d",
            aliases=["k_uz"],
            min=0.0001,
            max=1,
            mandatory=True,
        ),
        ParamSpec(
            name="alpha",
            unit="-",
            aliases=["alpha", "alfa"],
            min=0,
            max=3,
            default=1.0,
            mandatory=True,
        ),
    ],
    # HBV-96 capillary transport (upper zone -> soil moisture)
    "capillary:hbv": [
        ParamSpec(
            name="max_capillary_flux",
            unit="mm/d",
            aliases=["cflux"],
            min=0,
            max=3,
            default=0.0,
            mandatory=False,
        ),
    ],
    # HBV triangular unit hydrograph (MAXBAS)
    "routing:hbv": [
        ParamSpec(
            name="maxbas",
            unit="d",
            aliases=["maxbas"],
            min=1,
            max=10,
            default=1.0,
            mandatory=True,
        ),
    ],
    # Snowpack liquid water refreezing (degree-day; HBV)
    "refreeze:degree_day": [
        ParamSpec(
            name="refreezing_factor",
            unit="-",
            aliases=["cfr"],
            min=0,
            max=0.1,
            default=0.05,
            mandatory=False,
        ),
    ],
    # Snowpack liquid water holding capacity (HBV)
    "outflow:snow_holding": [
        ParamSpec(
            name="water_holding_capacity",
            unit="-",
            aliases=["cwh", "whc"],
            min=0,
            max=0.2,
            default=0.1,
            mandatory=False,
        ),
    ],
    # GR4J routing process (x2, x3, x4).
    "routing:gr4j": [
        ParamSpec(
            name="exchange_factor",
            unit="mm/d",
            aliases=["X2"],
            min=-10,
            max=5,
            default=0.0,
            mandatory=True,
        ),
        ParamSpec(
            name="routing_capacity",
            unit="mm",
            aliases=["X3"],
            min=1,
            max=500,
            default=90.0,
            mandatory=True,
        ),
        ParamSpec(
            name="uh_base_time",
            unit="d",
            aliases=["X4"],
            min=0.5,
            max=4,
            default=1.7,
            mandatory=True,
        ),
    ],
    # GR6J routing process (x2, x3, x4, x5, x6).
    "routing:gr6j": [
        ParamSpec(
            name="exchange_factor",
            unit="mm/d",
            aliases=["X2"],
            min=-10,
            max=5,
            default=0.0,
            mandatory=True,
        ),
        ParamSpec(
            name="routing_capacity",
            unit="mm",
            aliases=["X3"],
            min=1,
            max=500,
            default=90.0,
            mandatory=True,
        ),
        ParamSpec(
            name="uh_base_time",
            unit="d",
            aliases=["X4"],
            min=0.5,
            max=4,
            default=1.7,
            mandatory=True,
        ),
        ParamSpec(
            name="exchange_threshold",
            unit="-",
            aliases=["X5"],
            min=-2,
            max=2,
            default=0.0,
            mandatory=True,
        ),
        ParamSpec(
            name="exp_store_coeff",
            unit="mm",
            aliases=["X6"],
            min=0.05,
            max=20,
            default=4.0,
            mandatory=True,
        ),
    ],
    # Snow redistribution processes
    "transport:snow_slide": [
        ParamSpec(
            name="coeff",
            unit="-",
            aliases=["snow_slide_coeff"],
            min=0,
            max=10000,
            default=3178.4,
            mandatory=False,
        ),
        ParamSpec(
            name="exp",
            unit="-",
            aliases=["snow_slide_exp"],
            min=-5,
            max=0,
            default=-1.998,
            mandatory=False,
        ),
        ParamSpec(
            name="min_slope",
            unit="°",
            aliases=["snow_slide_min_slope"],
            min=0,
            max=45,
            default=10,
            mandatory=False,
        ),
        ParamSpec(
            name="max_slope",
            unit="°",
            aliases=["snow_slide_max_slope"],
            min=45,
            max=90,
            default=75,
            mandatory=False,
        ),
        ParamSpec(
            name="min_snow_holding_depth",
            unit="mm",
            aliases=["snow_slide_min_snow_depth"],
            min=0,
            max=1000,
            default=50,
            mandatory=False,
        ),
        ParamSpec(
            name="max_snow_depth",
            unit="mm",
            aliases=["snow_slide_max_snow_depth"],
            min=-1,
            max=50000,
            default=20000,
            mandatory=False,
        ),
    ],
    "transport:snow_redistribution_frey": [
        ParamSpec(
            name="correction",
            unit="-",
            aliases=["snow_redist_frey_c"],
            min=0,
            max=10,
            default=1.0,
            mandatory=False,
        ),
        ParamSpec(
            name="snow_holding_capacity",
            unit="mm",
            aliases=["snow_redist_frey_holding_capacity"],
            min=0,
            max=5000,
            default=200,
            mandatory=False,
        ),
        ParamSpec(
            name="rho_max",
            unit="kg/m3",
            aliases=["snow_redist_frey_rho_max"],
            min=200,
            max=600,
            default=450,
            mandatory=False,
        ),
        ParamSpec(
            name="snow_density",
            unit="kg/m3",
            aliases=["snow_redist_frey_snow_density"],
            min=50,
            max=600,
            default=250,
            mandatory=False,
        ),
        ParamSpec(
            name="max_snow_depth",
            unit="mm",
            aliases=["snow_redist_frey_max_snow_depth"],
            min=-1,
            max=50000,
            default=20000,
            mandatory=False,
        ),
    ],
    "transport:snow_redistribution_frey_dynamic": [
        ParamSpec(
            name="correction",
            unit="-",
            aliases=["snow_redist_frey_dyn_c"],
            min=0,
            max=10,
            default=1.0,
            mandatory=False,
        ),
        ParamSpec(
            name="snow_holding_capacity",
            unit="mm",
            aliases=["snow_redist_frey_dyn_holding_capacity"],
            min=0,
            max=5000,
            default=200,
            mandatory=False,
        ),
        ParamSpec(
            name="rho_max",
            unit="kg/m3",
            aliases=["snow_redist_frey_dyn_rho_max"],
            min=200,
            max=600,
            default=450,
            mandatory=False,
        ),
        ParamSpec(
            name="rho_min",
            unit="kg/m3",
            aliases=["snow_redist_frey_dyn_rho_min"],
            min=50,
            max=300,
            default=100,
            mandatory=False,
        ),
        ParamSpec(
            name="rho_fresh_max",
            unit="kg/m3",
            aliases=["snow_redist_frey_dyn_rho_fresh_max"],
            min=100,
            max=500,
            default=300,
            mandatory=False,
        ),
        ParamSpec(
            name="rho_settling",
            unit="1/day",
            aliases=["snow_redist_frey_dyn_rho_settling"],
            min=0,
            max=1,
            default=0.1,
            mandatory=False,
        ),
        ParamSpec(
            name="rho_scale",
            unit="-",
            aliases=["snow_redist_frey_dyn_rho_scale"],
            min=0.1,
            max=10,
            default=1.2,
            mandatory=False,
        ),
        ParamSpec(
            name="t_scale",
            unit="°C",
            aliases=["snow_redist_frey_dyn_t_scale"],
            min=-5,
            max=5,
            default=1.0,
            mandatory=False,
        ),
        ParamSpec(
            name="max_snow_depth",
            unit="mm",
            aliases=["snow_redist_frey_dyn_max_snow_depth"],
            min=-1,
            max=50000,
            default=20000,
            mandatory=False,
        ),
    ],
}

BRICK_PARAM_SPECS: dict[str, ParamSpec] = {
    "capacity": ParamSpec(
        name="capacity", unit="mm", aliases=[], min=0, max=3000, mandatory=True
    ),
}


# -----------------------------------------------------------------------------
# Validation & helper utilities for parameter specs
# -----------------------------------------------------------------------------
def validate_process_param_specs(specs: dict[str, list[ParamSpec]] | None = None):
    """Validate static process parameter specs.

    Checks:
    - No duplicate (process, parameter name) pairs.
    Alias duplication across different processes is allowed because processes
    (e.g., alternative melt formulations) are mutually exclusive at runtime
    and runtime registration still enforces global alias uniqueness.
    """
    if specs is None:
        specs = PROCESS_PARAM_SPECS

    seen_pairs: set[tuple[str, str]] = set()
    for proc, spec_list in specs.items():
        for spec in spec_list:
            pair = (proc, spec.name)
            if pair in seen_pairs:
                raise ConfigurationError(
                    f'Duplicate parameter name "{spec.name}" in process "{proc}".',
                    item_name=spec.name,
                    reason="Duplicate definition",
                )
            seen_pairs.add(pair)


def get_process_param_specs() -> dict[str, list[dict]]:
    """Return a JSON-serializable snapshot of the process parameter specs."""
    catalog: dict[str, list[dict]] = {}
    for proc, spec_list in PROCESS_PARAM_SPECS.items():
        catalog[proc] = [spec.to_kwargs() for spec in spec_list]
    return catalog


class ParameterSet:
    """Class for the parameter sets"""

    def __init__(self) -> None:
        """
        Initialize a ParameterSet instance.

        Sets up empty containers for parameters, constraints, and the list of
        parameters to assess during calibration.
        """
        self.parameters: pd.DataFrame = pd.DataFrame(
            columns=[
                "component",
                "name",
                "unit",
                "aliases",
                "value",
                "min",
                "max",
                "default",
                "mandatory",
                "prior",
                "transform",
            ]
        )
        self.constraints: list[list[str]] = []
        self._allow_changing: list[str] = []
        # Monthly-varying parameters: DataFrame index -> 12 monthly values (Jan..Dec).
        # The scalar 'value' still holds the annual mean as a baseline.
        self._monthly_values: dict[Hashable, list[float]] = {}
        # Spatial (per-unit) parameters: DataFrame index -> hydro-unit property name.
        # The scalar 'value' is kept as a fallback for units lacking the property.
        self._spatial: dict[Hashable, str] = {}

    @property
    def allow_changing(self) -> list[str]:
        """
        Get the list of parameters to assess during calibration.

        Returns
        -------
        list[str]
            List of parameter names that are allowed to change.
        """
        return self._allow_changing

    @allow_changing.setter
    def allow_changing(self, allow_changing: list[str]) -> None:
        """
        Set the list of parameters to assess.

        Parameters
        ----------
        allow_changing
            A list of parameters to assess. Only the parameters in this list will be
            changed. If a parameter is related to data forcing, the spatialization
            will be performed again.
        """
        self._allow_changing = allow_changing

    def define_parameter(
        self,
        component: str,
        name: str,
        unit: str | None = None,
        aliases: str | list[str] | None = None,
        min_val: float | list[float] | None = None,
        max_val: float | list[float] | None = None,
        default: float | list[float] | None = None,
        mandatory: bool = True,
    ) -> None:
        """
        Define a parameter by setting its properties.

        Parameters
        ----------
        component
            The component (brick) name to which the parameter refer (e.g., snowpack,
            glacier, surface_runoff). It can be a string of a list of components when
            the parameter is shared between components (e.g., melt_factor in the
            temperature index method).
        name
            The name of the parameter in the C++ code of hydrobricks (e.g.,
            degree_day_factor, response_factor).
        unit
            The unit of the parameter.
        aliases
            Aliases to the parameter name, such as names used in other implementations
            (e.g., kgl, an). Aliases must be unique.
        min_val
            Minimum value allowed for the parameter.
        max_val
            Maximum value allowed for the parameter.
        default
            The parameter default value.
        mandatory
            If the parameter needs to be defined or if it can silently use the
            default value.
        """
        value = None
        if not mandatory and default is not None:
            value = default

        self._check_aliases_uniqueness(aliases)
        self._check_min_max_consistency(min_val, max_val)

        new_row = pd.Series(
            {
                "component": component,
                "name": name,
                "unit": unit,
                "aliases": aliases,
                "value": value,
                "min": min_val,
                "max": max_val,
                "default": default,
                "mandatory": mandatory,
                "prior": None,
                "transform": None,
            }
        )

        self.parameters = pd.concat(
            [self.parameters, new_row.to_frame().T], ignore_index=True
        )

    def add_aliases(self, parameter_name: str, aliases: list[str] | str) -> None:
        """
        Add aliases to a parameter.

        Parameters
        ----------
        parameter_name
            The name of the parameter with the related component (e.g.,
            snowpack:degree_day_factor).
        aliases
            Aliases to the parameter name, such as names used in other implementations
            (e.g., kgl, an). Aliases must be unique.
        """
        if not isinstance(aliases, list):
            aliases = [aliases]
        index = self._get_parameter_index(parameter_name)
        current = self.parameters.at[index, "aliases"]
        if current is None:
            self.parameters.at[index, "aliases"] = aliases
        else:
            self.parameters.at[index, "aliases"] = current + aliases

    def change_range(
        self,
        parameter: str,
        min_val: float | None = None,
        max_val: float | None = None,
    ) -> None:
        """
        Change the value range of a parameter.

        Only the bounds that are provided are changed; passing ``None`` (the
        default) leaves that bound untouched. This allows raising a lower bound
        without having to restate the maximum.

        Parameters
        ----------
        parameter
            Name (or alias) of the parameter
        min_val
            New minimum value, or None to keep the current minimum.
        max_val
            New maximum value, or None to keep the current maximum.
        """
        index = self._get_parameter_index(parameter)
        if min_val is not None:
            self.parameters.loc[index, "min"] = min_val
        if max_val is not None:
            self.parameters.loc[index, "max"] = max_val

    def set_transform(
        self,
        parameter: str,
        to_transformed: Callable[[float], float],
        to_real: Callable[[float], float],
    ) -> None:
        """
        Attach a transform between the real and transformed values of a parameter.

        The real value (used by the C++ engine and stored in the parameter set) and
        the transformed value (used for optimisation or to express the parameter in
        its original formulation) are related by the two provided functions. Either
        representation can be set; the other is derived. Both functions must be
        monotonic.

        Parameters
        ----------
        parameter
            Name (or alias) of the parameter.
        to_transformed
            Function mapping the real value to the transformed value.
        to_real
            Function mapping the transformed value back to the real value.

        Raises
        ------
        ConfigurationError
            If the parameter is list-valued (transforms are not supported for lists).
        """
        index = self._get_parameter_index(parameter)
        if isinstance(self.parameters.loc[index, "min"], list) or isinstance(
            self.parameters.loc[index, "value"], list
        ):
            raise ConfigurationError(
                "Transforms on list-valued parameters are not supported.",
                item_name=parameter,
                reason="Unsupported parameter type",
            )
        self.parameters.at[index, "transform"] = ParameterTransform(
            to_transformed, to_real
        )

    def set_prior(self, parameter: str, prior: spotpy.parameter) -> None:
        """
        Set a prior distribution for a parameter.

        Assigns a prior probability distribution to a parameter for use in Bayesian
        calibration methods.

        Parameters
        ----------
        parameter
            Name (or alias) of the parameter
        prior
            The prior distribution (instance of spotpy.parameter)

        Raises
        ------
        ImportError
            If spotpy is not installed.
        """
        if not HAS_SPOTPY:
            raise DependencyError(
                "spotpy is required for parameter distributions.",
                package_name="spotpy",
                operation="ParameterSet.set_prior",
                install_command="pip install spotpy",
            )

        index = self._get_parameter_index(parameter)
        prior.name = parameter
        self.parameters.loc[index, "prior"] = prior

    def list_constraints(self) -> None:
        """
        List the constraints currently defined.

        Prints all defined parameter constraints to the console.
        """
        for constraint in self.constraints:
            print(" ".join(constraint))

    def define_constraint(
        self, parameter_1: str, operator: str, parameter_2: str
    ) -> None:
        """
        Defines a constraint between 2 parameters (e.g., paramA > paramB)

        Parameters
        ----------
        parameter_1
            The name of the first parameter.
        operator
            The operator (e.g. '<=').
        parameter_2
            The name of the second parameter.

        Examples
        --------
        parameter_set.define_constraint('paramA', '>=', 'paramB')
        """
        constraint = [parameter_1, operator, parameter_2]
        self.constraints.append(constraint)

    def remove_constraint(
        self, parameter_1: str, operator: str, parameter_2: str
    ) -> None:
        """
        Removes a constraint between 2 parameters (e.g., paramA > paramB)

        Parameters
        ----------
        parameter_1
            The name of the first parameter.
        operator
            The operator (e.g. '<=').
        parameter_2
            The name of the second parameter.

        Examples
        --------
        parameter_set.remove_constraint('paramA', '>=', 'paramB')
        """
        for i, constraint in enumerate(self.constraints):
            if (
                parameter_1 == constraint[0]
                and operator == constraint[1]
                and parameter_2 == constraint[2]
            ):
                del self.constraints[i]
                return

    def constraints_satisfied(self) -> bool:
        """
        Check if the constraints between parameters are satisfied.

        Returns
        -------
        bool
            True if constraints are satisfied, False otherwise.
        """
        for constraint in self.constraints:
            # Ignore constraints involving unused parameters
            if not self.has(constraint[0]) or not self.has(constraint[2]):
                continue

            val_1 = self.get(constraint[0])
            operator = constraint[1]
            val_2 = self.get(constraint[2])

            if isinstance(val_1, list) or isinstance(val_2, list):
                raise ConfigurationError(
                    "Constraints involving list parameters are not yet implemented.",
                    reason="Feature not yet implemented",
                )

            if operator in [">", "gt"]:
                if val_1 <= val_2:
                    return False
            elif operator in [">=", "ge"]:
                if val_1 < val_2:
                    return False
            elif operator in ["<", "lt"]:
                if val_1 >= val_2:
                    return False
            elif operator in ["<=", "le"]:
                if val_1 > val_2:
                    return False

        return True

    def range_satisfied(self) -> bool:
        """
        Check if the parameter value ranges are satisfied.

        Returns
        -------
        bool
            True if ranges are satisfied, False otherwise.
        """
        for _, row in self.parameters.iterrows():
            min_val = row["min"]
            max_val = row["max"]
            value = row["value"]

            if value is None:
                return False

            if not isinstance(min_val, list):
                if max_val is not None and value > max_val:
                    return False
                if min_val is not None and value < min_val:
                    return False
            else:
                assert isinstance(max_val, list)
                assert isinstance(value, list)
                for min_v, max_v, val in zip(min_val, max_val, value):
                    if max_v is not None and val > max_v:
                        return False
                    if min_v is not None and val < min_v:
                        return False

        return True

    def set_values(
        self,
        values: dict,
        check_range: bool = True,
        allow_adapt: bool = False,
        transformed: bool = False,
    ) -> None:
        """
        Set the parameter values.

        Parameters
        ----------
        values
            The values must be provided as a dictionary with the parameter name with the
            related component or one of its aliases as the key.
            Example: {'k': 32, 'A': 300} or {'slow_reservoir:capacity': 300}
        check_range
            Check that the parameter value falls into the allowed range.
        allow_adapt
            Allow the parameter values to be adapted to enforce defined constraints
            (e.g.: min, max).
        transformed
            If True, the provided values are in transformed space and are mapped back
            to the real value (using each parameter's transform) before being stored.
            Parameters without a transform are treated as real values. The range check
            and storage always operate on the real value.
        """
        for key in values:
            index = self._get_parameter_index(key)
            value = values[key]
            if transformed:
                transform = self._get_transform(index)
                if transform is not None:
                    value = transform.to_real(value)
            if check_range:
                value = self._check_value_range(
                    index, key, value, allow_adapt=allow_adapt
                )
            self.parameters.loc[index, "value"] = value

    def set_monthly_values(
        self,
        name: str,
        values: list[float],
        check_range: bool = True,
        allow_adapt: bool = False,
    ) -> None:
        """
        Set 12 monthly values for a parameter (time-varying over the calendar year).

        The parameter value then follows the calendar month during the simulation
        (e.g. a monthly canopy interception capacity). The scalar value is set to the
        annual mean as a baseline (used before the first monthly update and by the
        parameter-validity check).

        Parameters
        ----------
        name
            The parameter name or one of its aliases.
        values
            The 12 monthly values, from January to December.
        check_range
            Check that each monthly value falls into the allowed range.
        allow_adapt
            Allow the monthly values to be clipped to the allowed range.
        """
        if len(values) != 12:
            raise ConfigurationError(
                f'Monthly values for "{name}" must have 12 entries (got '
                f"{len(values)}).",
                item_name=name,
                item_value=len(values),
                reason="Expected 12 monthly values",
            )

        index = self._get_parameter_index(name)
        checked = list(values)
        if check_range:
            checked = [
                self._check_value_range(index, name, float(v), allow_adapt=allow_adapt)
                for v in checked
            ]

        self._monthly_values[index] = checked
        # Keep the scalar value as the annual mean so validity checks pass and any
        # non-monthly consumer still sees a sensible value.
        self.parameters.loc[index, "value"] = sum(checked) / 12.0

    def get_monthly_parameters(self) -> list[tuple[str, str, list[float]]]:
        """
        Return the monthly-varying model parameters.

        Returns
        -------
        A list of ``(component, name, values)`` tuples (one per monthly parameter),
        used to push the monthly values to the model settings. Data parameters are
        excluded.
        """
        monthly = []
        for index, values in self._monthly_values.items():
            row = self.parameters.loc[index]
            if row["component"] == "data":
                continue
            monthly.append((row["component"], row["name"], values))
        return monthly

    def set_spatial(self, name: str, property_name: str) -> None:
        """
        Make a parameter spatial: each hydro unit uses its own value from a property.

        The per-unit values are taken from the hydro-unit property ``property_name``
        (added via ``HydroUnits.add_property``), giving each unit its own value for this
        parameter (e.g. a per-unit field capacity from soil data). The scalar value set
        via ``set_values`` is kept as a fallback for units lacking the property, so it
        must still be defined.

        Parameters
        ----------
        name
            The parameter name or one of its aliases.
        property_name
            The name of the hydro-unit property holding the per-unit values.
        """
        index = self._get_parameter_index(name)
        self._spatial[index] = property_name

    def get_spatial_parameters(self) -> list[tuple[str, str, str]]:
        """
        Return the spatial (per-unit) model parameters.

        Returns
        -------
        A list of ``(component, name, property)`` tuples (one per spatial parameter),
        used to push the bindings to the model settings. Data parameters are excluded.
        """
        spatial = []
        for index, property_name in self._spatial.items():
            row = self.parameters.loc[index]
            if row["component"] == "data":
                continue
            spatial.append((row["component"], row["name"], property_name))
        return spatial

    def get_transform(self, name: str) -> ParameterTransform | None:
        """
        Return the transform attached to a parameter, or None.

        Parameters
        ----------
        name
            The name or one of the aliases of the parameter.

        Returns
        -------
        ParameterTransform | None
            The transform if one is set, otherwise None. Parameters without a
            transform (or unknown names) return None.
        """
        index = self._get_parameter_index(name, raise_exception=False)
        if index is None:
            return None
        return self._get_transform(index)

    def has(self, name: str) -> bool:
        """
        Check if a parameter exists.

        Parameters
        ----------
        name
            The name of the parameter.

        Returns
        -------
        bool
            True if found, False otherwise.
        """
        index = self._get_parameter_index(name, raise_exception=False)
        return index is not None

    def get(self, name: str) -> float:
        """
        Get the value of a parameter by name.

        Parameters
        ----------
        name
            The name of the parameter.

        Returns
        -------
        float
            The parameter value.
        """
        index = self._get_parameter_index(name)
        return self.parameters.loc[index, "value"]

    def get_transformed(self, name: str) -> float:
        """
        Get the transformed value of a parameter by name.

        Returns the parameter value mapped through its transform (if any). If the
        parameter has no transform, the real value is returned unchanged.

        Parameters
        ----------
        name
            The name of the parameter.

        Returns
        -------
        float
            The transformed parameter value.
        """
        index = self._get_parameter_index(name)
        value = self.parameters.loc[index, "value"]
        transform = self._get_transform(index)
        return value if transform is None else transform.to_transformed(value)

    def are_valid(self) -> bool:
        """
        Check if all the parameters are defined and have a value.
        Alias of is_valid.

        Returns
        -------
        bool
            True if all parameters are defined and have a value, False otherwise.
        """
        return self.is_valid()

    def is_valid(self) -> bool:
        """
        Check if all the parameters are defined and have a value.

        Returns
        -------
        bool
            True if all parameters are defined and have a value, False otherwise.
        """
        for _, row in self.parameters.iterrows():
            if row["value"] is None:
                return False
        return True

    def get_undefined(self) -> list[str]:
        """
        Get the undefined parameters.

        Returns
        -------
        list[str]
            List of the undefined parameter names.
        """
        undefined = []
        for _, row in self.parameters.iterrows():
            if row["value"] is None:
                undefined.append(row["name"])
        return undefined

    def get_model_parameters(self) -> pd.DataFrame:
        """
        Get the model-only parameters (excluding data-related parameters).

        Returns
        -------
        pd.DataFrame
            DataFrame containing model parameters only.
        """
        return self.parameters[self.parameters["component"] != "data"]

    def add_data_parameter(
        self,
        name: str,
        value: float | list[float] | None = None,
        min_val: float | list[float] | None = None,
        max_val: float | list[float] | None = None,
        unit: str | None = None,
    ) -> None:
        """
        Add a parameter related to the data.

        Parameters
        ----------
        name
            The name of the parameter.
        value
            The parameter value.
        min_val
            Minimum value allowed for the parameter.
        max_val
            Maximum value allowed for the parameter.
        unit
            The unit of the parameter.
        """
        aliases = [name]

        self._check_aliases_uniqueness(aliases)
        self._check_min_max_consistency(min_val, max_val)

        new_row = pd.Series(
            {
                "component": "data",
                "name": name,
                "unit": unit,
                "aliases": aliases,
                "value": value,
                "min": min_val,
                "max": max_val,
                "default": value,
                "mandatory": False,
            }
        )

        self.parameters = pd.concat(
            [self.parameters, new_row.to_frame().T], ignore_index=True
        )

    def is_for_forcing(self, parameter_name: str) -> bool:
        """
        Check if the parameter relates to forcing data.

        Parameters
        ----------
        parameter_name
            The name of the parameter.

        Returns
        -------
        bool
            True if relates to forcing data, False otherwise.
        """
        index = self._get_parameter_index(parameter_name)
        return self.parameters.loc[index, "component"] == "data"

    def set_random_values(self, parameters: list[str]) -> pd.DataFrame:
        """
        Set the provided parameter to random values.

        Randomly assigns values to specified parameters within their defined ranges.
        Iterates until all constraints are satisfied.

        Parameters
        ----------
        parameters
            The name or alias of the parameters to set to random values.
            Example: ['kr', 'A']

        Returns
        -------
        pd.DataFrame
            A dataframe with the assigned parameter values.

        Raises
        ------
        ValueError
            If parameter constraints cannot be satisfied after 1000 iterations.
        """

        # Create a dataframe to return assigned values
        assigned_values = pd.DataFrame(columns=parameters)

        for i in range(1100):
            for key in parameters:
                index = self._get_parameter_index(key)
                min_val = self.parameters.loc[index, "min"]
                max_val = self.parameters.loc[index, "max"]

                if isinstance(min_val, list):
                    if self.parameters.loc[index, "value"] is None:
                        self.parameters.loc[index, "value"] = [0] * len(min_val)
                    for (idx, min_v), max_v in zip(enumerate(min_val), max_val):
                        self.parameters.loc[index, "value"][idx] = random.uniform(
                            min_v, max_v
                        )
                else:
                    self.parameters.loc[index, "value"] = random.uniform(
                        min_val, max_val
                    )

                if isinstance(min_val, list):
                    assigned_values.loc[0, key] = self.parameters.loc[
                        index, "value"
                    ].copy()
                else:
                    assigned_values.loc[0, key] = self.parameters.loc[index, "value"]

            if self.constraints_satisfied():
                break

            if i >= 1000:
                raise ConfigurationError(
                    "The parameter constraints could not be "
                    "satisfied after 1000 iterations."
                )

        return assigned_values

    def needs_random_forcing(self) -> bool:
        """
        Check if one of the parameters to assess involves the meteorological data.

        Returns
        -------
        True if one of the parameters to assess involves the meteorological data.
        """
        for param in self.allow_changing:
            if not self.has(param):
                raise ConfigurationError(
                    f"The parameter {param} was not found.",
                    item_name=param,
                    reason="Parameter not found",
                )
            if self.is_for_forcing(param):
                return True
        return False

    def get_for_spotpy(self) -> list[spotpy.parameter]:
        """
        Get the parameters to assess ready to be used in spotpy.

        Returns
        -------
        A list of the parameters as spotpy objects.
        """
        if not HAS_SPOTPY:
            raise DependencyError(
                "spotpy is required for parameter optimization.",
                package_name="spotpy",
                operation="ParameterSet.get_for_spotpy",
                install_command="pip install spotpy",
            )

        spotpy_params = []
        for param_name in self.allow_changing:
            index = self._get_parameter_index(param_name)
            param = self.parameters.loc[index]
            # The prior is either unset (NaN) or a spotpy parameter object; pd.isna
            # handles both (np.isnan would raise on the object).
            if not pd.isna(param["prior"]):
                # A supplied prior is assumed to already be in optimizer space.
                spotpy_params.append(param["prior"])
            else:
                low, high = param["min"], param["max"]
                transform = self._get_transform(index)
                if transform is not None:
                    # The optimizer searches in transformed space; map the real
                    # bounds through the transform (sorted to handle decreasing maps).
                    low, high = sorted(
                        [transform.to_transformed(low), transform.to_transformed(high)]
                    )
                spotpy_params.append(
                    spotpy.parameter.Uniform(param_name, low=low, high=high)
                )

        return spotpy_params

    def save_as(self, directory: str, name: str, file_type: str = "both"):
        """
        Create a configuration file containing the parameter values.

        Such a file can be used when using the command-line version of hydrobricks. It
        contains the model parameter values.

        Parameters
        ----------
        directory
            The directory to write the file.
        name
            The name of the generated file.
        file_type
            The type of file to generate: 'json', 'yaml', or 'both'.
        """
        grouped_params = self.parameters.groupby("component", sort=False)
        file_content = {}

        for group_name, group in grouped_params:
            group_content = {}
            for _, row in group.iterrows():
                group_content.update({row["name"]: row["value"]})
            file_content.update({group_name: group_content})

        dump_config_file(file_content, directory, name, file_type)

    def generate_parameters(
        self,
        land_cover_types: list[str],
        land_cover_names: list[str],
        options: dict,
        structure: dict,
    ):
        """
        Generate a parameters object for the provided model options and structure.

        Parameters
        ----------
        land_cover_types
            The land cover types.
        land_cover_names
            The land cover names.
        options
            The model options.
        structure
            The model structure.
        """
        # General parameters
        self._generate_snow_parameters(options, land_cover_types, land_cover_names)
        self._generate_precipitation_correction_parameters(options)

        # Parameters for the glaciers
        self._generate_glacier_parameters(land_cover_types, land_cover_names, structure)

        # Parameters for the forest canopies (interception capacity), only when the
        # canopy interception is enabled (the canopy bricks are then generated).
        if options.get("forest_interception", False):
            self._generate_canopy_parameters(land_cover_types, land_cover_names)

        # Parameters for the different bricks
        for key, brick in structure.items():
            self._generate_brick_parameters(key, brick)
            self._generate_process_parameters(key, brick)

    def _register(
        self,
        component: str,
        spec: ParamSpec,
        alias_suffix: str = "",
        **overrides: dict,
    ) -> None:
        """Register a parameter based on a ParamSpec.

        Parameters
        ----------
        component: str
            Component name used in define_parameter.
        spec: ParamSpec
            The static specification.
        alias_suffix: str
            Suffix appended to every alias of the spec (e.g. '_forest'). Used when
            the same process/brick parameter exists on several land covers, so the
            literature aliases (beta, lp, ...) stay unique per cover. Empty by
            default (single occurrence keeps the bare alias).
        overrides: dict
            Any field accepted by define_parameter to override spec values.
        """
        kwargs = spec.to_kwargs()
        # Apply overrides if supplied
        for key, val in overrides.items():
            if key in kwargs:
                kwargs[key] = val
        if alias_suffix and kwargs.get("aliases"):
            kwargs["aliases"] = [alias + alias_suffix for alias in kwargs["aliases"]]
        self.define_parameter(component=component, **kwargs)

    def _generate_canopy_parameters(
        self, land_cover_types: list, land_cover_names: list
    ) -> None:
        """Register the interception capacity of each forest canopy.

        The canopy bricks (``<cover>_canopy``) are created in the C++ base structure
        (like the snowpacks), so their capacity is registered here rather than from
        the model structure dict. The literature alias is ``ic`` (interception
        capacity), suffixed per cover (``ic_<cover>``) when several forests coexist.

        Parameters
        ----------
        land_cover_types
            The land cover types.
        land_cover_names
            The land cover names.
        """
        forest_covers = [
            name
            for cover_type, name in zip(land_cover_types, land_cover_names)
            if cover_type == "forest"
        ]
        multi = len(forest_covers) > 1
        for cover_name in forest_covers:
            self._register(
                component=f"{cover_name}_canopy",
                spec=BRICK_PARAM_SPECS["capacity"],
                alias_suffix=f"_{cover_name}" if multi else "",
                aliases=["ic"],
                min_val=0.0,
                max_val=10.0,
                default=2.0,
                mandatory=False,
            )

    def _generate_process_parameters(self, key: str, brick: dict) -> None:
        """
        Register parameters for all processes in a brick.

        Parameters
        ----------
        key
            The brick component name.
        brick
            Dictionary containing the brick structure with 'processes' key.
        """
        if "processes" not in brick:
            return

        alias_suffix = brick.get("alias_suffix", "")

        skip = {
            # No parameters
            "infiltration:socont",
            "outflow:rest",
            "outflow:direct",
            "et:socont",
            "et:open_water",
            "et:linear",
            "overflow",
            "interception:gr4j",
            "infiltration:gr4j",
            "production:gr4j",
            "et:gr4j",
            "percolation:gr4j",
            # Defined elsewhere (glacier/snow generation logic)
            "melt:degree_day",
            "melt:degree_day_aspect",
            "melt:degree_day_seasonal",
            "melt:temperature_index",
            "melt:cemaneige",
        }

        for _, process in brick["processes"].items():
            kind = process["kind"]
            if kind in skip:
                continue
            if kind in PROCESS_PARAM_SPECS:
                for spec in PROCESS_PARAM_SPECS[kind]:
                    self._register(component=key, spec=spec, alias_suffix=alias_suffix)
            else:
                raise ConfigurationError(
                    f"The process {kind} is not recognised in parameters generation.",
                    item_value=kind,
                    reason="Unknown process type",
                )

    def _generate_brick_parameters(self, key: str, brick: dict) -> None:
        """
        Register parameters defined in a brick structure.

        Parameters
        ----------
        key
            The brick component name.
        brick
            Dictionary containing the brick structure with 'parameters' key.
        """
        if "parameters" not in brick:
            return

        alias_suffix = brick.get("alias_suffix", "")
        skip = {"no_melt_when_snow_cover", "infinite_storage"}

        for param_name, _ in brick["parameters"].items():
            if param_name in skip:
                continue
            if param_name in BRICK_PARAM_SPECS:
                self._register(
                    component=key,
                    spec=BRICK_PARAM_SPECS[param_name],
                    alias_suffix=alias_suffix,
                )
            else:
                raise ConfigurationError(
                    f"Parameter {param_name} is not recognised in params generation.",
                    item_name=param_name,
                    reason="Unknown parameter",
                )

    def _generate_glacier_parameters(
        self, land_cover_types: list[str], land_cover_names: list[str], structure: dict
    ) -> None:
        """
        Register parameters for glacier processes.

        Generates parameters specific to glacier melt methods, handling multiple
        glaciers with appropriate aliases and component names.

        Parameters
        ----------
        land_cover_types
            List of land cover types (e.g., 'glacier', 'ground').
        land_cover_names
            List of land cover names corresponding to types.
        structure
            Model structure dictionary containing glacier configuration.
        """
        if "glacier" not in land_cover_types:
            return

        glacier_names = [
            cover_name
            for cover_type, cover_name in zip(land_cover_types, land_cover_names)
            if cover_type == "glacier"
        ]

        for i, cover_name in enumerate(glacier_names):
            melt_method = structure[cover_name]["processes"]["melt"]["kind"]

            if len(glacier_names) == 1:
                t_aliases = ["melt_t_ice"]
            else:
                t_aliases = [
                    f'melt_t_ice_{cover_name.replace("-", "_")}',
                    f"melt_t_ice_{i}",
                ]

            if melt_method not in PROCESS_PARAM_SPECS:
                raise ConfigurationError(
                    f"The glacier melt method {melt_method} is not recognised "
                    f"in parameters generation.",
                    item_name="melt_method",
                    item_value=melt_method,
                    reason="Unknown melt method",
                )

            # Build dynamic alias mapping per parameter name
            alias_map: dict[str, list[str]] = {}
            if melt_method == "melt:degree_day":
                if len(glacier_names) == 1:
                    a_aliases = ["a_ice"]
                else:
                    a_aliases = [f'a_ice_{cover_name.replace("-", "_")}', f"a_ice_{i}"]
                alias_map = {
                    "degree_day_factor": a_aliases,
                    "melting_temperature": t_aliases,
                }
            elif melt_method == "melt:degree_day_aspect":
                if len(glacier_names) == 1:
                    a_n_aliases = ["a_ice_n"]
                    a_s_aliases = ["a_ice_s"]
                    a_ew_aliases = ["a_ice_ew"]
                else:
                    a_n_aliases = [
                        f'a_ice_n_{cover_name.replace("-", "_")}',
                        f"a_ice_n_{i}",
                    ]
                    a_s_aliases = [
                        f'a_ice_s_{cover_name.replace("-", "_")}',
                        f"a_ice_s_{i}",
                    ]
                    a_ew_aliases = [
                        f'a_ice_ew_{cover_name.replace("-", "_")}',
                        f"a_ice_ew_{i}",
                    ]
                alias_map = {
                    "degree_day_factor_n": a_n_aliases,
                    "degree_day_factor_s": a_s_aliases,
                    "degree_day_factor_ew": a_ew_aliases,
                    "melting_temperature": t_aliases,
                }
            elif melt_method == "melt:degree_day_seasonal":
                if len(glacier_names) == 1:
                    a_min_aliases = ["a_ice_min"]
                    a_max_aliases = ["a_ice_max"]
                else:
                    a_min_aliases = [
                        f'a_ice_min_{cover_name.replace("-", "_")}',
                        f"a_ice_min_{i}",
                    ]
                    a_max_aliases = [
                        f'a_ice_max_{cover_name.replace("-", "_")}',
                        f"a_ice_max_{i}",
                    ]
                alias_map = {
                    "degree_day_factor_min": a_min_aliases,
                    "degree_day_factor_max": a_max_aliases,
                    "melting_temperature": t_aliases,
                }
            elif melt_method == "melt:temperature_index":
                if len(glacier_names) == 1:
                    r_aliases = ["r_ice"]
                else:
                    r_aliases = [f'r_ice_{cover_name.replace("-", "_")}', f"r_ice_{i}"]
                alias_map = {
                    "radiation_coefficient": r_aliases,
                    "melting_temperature": t_aliases,
                }

            # Register parameters from specs with aliases
            for spec in PROCESS_PARAM_SPECS[melt_method]:
                if spec.name == "melt_factor":  # already registered for snow & glacier
                    continue
                self._register(
                    component=cover_name,
                    spec=spec,
                    aliases=alias_map.get(spec.name, []),
                )

            with_glacier_debris = (
                len(glacier_names) > 1 and cover_name == "glacier_debris"
            )
            # Constraints
            if melt_method == "melt:degree_day":
                if with_glacier_debris:
                    self.define_constraint(
                        "a_ice_glacier_debris", "<", "a_ice_glacier_ice"
                    )
                self.define_constraint("a_snow", "<", alias_map["degree_day_factor"][0])
            elif melt_method == "melt:degree_day_aspect":
                if with_glacier_debris:
                    self.define_constraint(
                        "a_ice_n_glacier_debris", "<", "a_ice_n_glacier_ice"
                    )
                    self.define_constraint(
                        "a_ice_s_glacier_debris", "<", "a_ice_s_glacier_ice"
                    )
                    self.define_constraint(
                        "a_ice_ew_glacier_debris", "<", "a_ice_ew_glacier_ice"
                    )
                self.define_constraint(
                    "a_snow", "<", alias_map["degree_day_factor_n"][0]
                )
                self.define_constraint(
                    "a_snow", "<", alias_map["degree_day_factor_s"][0]
                )
                self.define_constraint(
                    "a_snow", "<", alias_map["degree_day_factor_ew"][0]
                )
                self.define_constraint(
                    "a_snow_n", "<", alias_map["degree_day_factor_n"][0]
                )
                self.define_constraint(
                    "a_snow_s", "<", alias_map["degree_day_factor_s"][0]
                )
                self.define_constraint(
                    "a_snow_ew", "<", alias_map["degree_day_factor_ew"][0]
                )
            elif melt_method == "melt:degree_day_seasonal":
                if with_glacier_debris:
                    self.define_constraint(
                        "a_ice_min_glacier_debris", "<", "a_ice_min_glacier_ice"
                    )
                    self.define_constraint(
                        "a_ice_max_glacier_debris", "<", "a_ice_max_glacier_ice"
                    )
                self.define_constraint(
                    "a_snow_min", "<", alias_map["degree_day_factor_min"][0]
                )
                self.define_constraint(
                    "a_snow_max", "<", alias_map["degree_day_factor_max"][0]
                )
            elif melt_method == "melt:temperature_index":
                if with_glacier_debris:
                    self.define_constraint(
                        "r_ice_glacier_debris", "<", "r_ice_glacier_ice"
                    )
                self.define_constraint(
                    "r_snow", "<", alias_map["radiation_coefficient"][0]
                )

    def _generate_precipitation_correction_parameters(self, options: dict) -> None:
        """Register the optional precipitation correction factors.

        The correction factors (HBV RFCF/SFCF) live on the precipitation splitter
        and default to 1.0 (no correction). The relevant splitter depends on
        whether the model has snow: the snow/rain splitter ('snow_rain_transition')
        carries both the rain and snow factors, whereas the rain-only splitter
        ('rain', used by no-snow models) carries only the rain factor. This mirrors
        the splitter generation in SettingsModel::GeneratePrecipitationSplitters.

        Parameters
        ----------
        options
            Model options dictionary (uses 'snow_melt_process'/'snow_rain_process'
            to decide whether snow is configured).
        """
        srp = options.get("snow_rain_process")
        smp = options.get("snow_melt_process")
        with_snow = srp is not None or smp is not None

        if with_snow:
            for spec in PROCESS_PARAM_SPECS["correction:snow_rain"]:
                self._register(component="snow_rain_transition", spec=spec)
            # Snow undercatch is usually at least as large as rain undercatch, so the
            # snow factor should not fall below the rain factor. Applies to every
            # snow-bearing model (only the snow/rain splitter carries both factors).
            self.define_constraint(
                "rain_correction_factor", "<=", "snow_correction_factor"
            )
        else:
            for spec in PROCESS_PARAM_SPECS["correction:rain"]:
                self._register(component="rain", spec=spec)

    def _generate_snow_parameters(
        self, options: dict, land_cover_types: list[str], land_cover_names: list[str]
    ) -> None:
        """
        Register parameters for snow processes.

        Generates parameters for snow melt and redistribution based on model options,
        handling multiple snow types with appropriate aliases.

        Parameters
        ----------
        options
            Model options dictionary containing snow configuration.
        land_cover_types
            List of land cover types (e.g., 'glacier', 'ground').
        land_cover_names
            List of land cover names corresponding to types.
        """
        if "snow_melt_process" in options or "with_snow" in options:
            # Snow/rain transition specs (pseudo-process)
            srp = options.get("snow_rain_process")
            smp = options.get("snow_melt_process")
            if srp is None and smp is None:
                return  # no snow configured at all
            if srp == "snow_rain:threshold":
                transition_key = "transition:snow_rain:threshold"
            elif srp == "snow_rain:cemaneige" or smp == "melt:cemaneige":
                transition_key = None  # no calibrated params — uses Tmin/Tmax forcings
            else:
                transition_key = "transition:snow_rain:linear"
            if transition_key is not None:
                for spec in PROCESS_PARAM_SPECS[transition_key]:
                    self._register(component="snow_rain_transition", spec=spec)

            if "snow_melt_process" in options:
                smp = options["snow_melt_process"]
                if smp is None:
                    return
                if smp in PROCESS_PARAM_SPECS:
                    snow_alias_map: dict[str, list[str]] = {}
                    if smp == "melt:degree_day":
                        snow_alias_map = {
                            "degree_day_factor": ["a_snow"],
                            "melting_temperature": ["melt_t_snow"],
                        }
                    elif smp == "melt:degree_day_aspect":
                        snow_alias_map = {
                            "degree_day_factor_n": ["a_snow_n"],
                            "degree_day_factor_s": ["a_snow_s"],
                            "degree_day_factor_ew": ["a_snow_ew"],
                            "melting_temperature": ["melt_t_snow"],
                        }
                    elif smp == "melt:degree_day_seasonal":
                        snow_alias_map = {
                            "degree_day_factor_min": ["a_snow_min", "crmfmin"],
                            "degree_day_factor_max": ["a_snow_max", "crmfmax"],
                            "melting_temperature": ["melt_t_snow"],
                        }
                    elif smp == "melt:temperature_index":
                        snow_alias_map = {
                            "melt_factor": ["melt_factor", "mf"],
                            "radiation_coefficient": ["r_snow"],
                            "melting_temperature": ["melt_t_snow"],
                        }
                    elif smp == "melt:cemaneige":
                        snow_alias_map = {
                            "degree_day_factor": ["Kf"],
                            "cold_content_factor": ["CTG"],
                            "melting_temperature": ["Tmelt"],
                            "mean_annual_snow": ["Cn"],
                        }
                    for spec in PROCESS_PARAM_SPECS[smp]:
                        component = "type:snowpack"
                        if spec.name == "melt_factor":
                            # Shared between snowpack & glacier
                            component = "type:snowpack,type:glacier"
                        self._register(
                            component=component,
                            spec=spec,
                            aliases=snow_alias_map.get(spec.name, []),
                        )
                    if smp == "melt:degree_day_seasonal":
                        self.define_constraint("a_snow_min", "<", "a_snow_max")
                else:
                    raise ConfigurationError(
                        f"The snow melt process option {smp} is not recognised.",
                        item_name="snow_melt_process",
                        item_value=smp,
                        reason="Unknown process",
                    )

            # Snowpack liquid water retention and refreezing (e.g., HBV)
            for option_key in [
                "snow_water_retention_process",
                "snow_refreezing_process",
            ]:
                process = options.get(option_key)
                if process is None:
                    continue
                if process not in PROCESS_PARAM_SPECS:
                    raise ConfigurationError(
                        f"The {option_key} option {process} is not recognised.",
                        item_name=option_key,
                        item_value=process,
                        reason="Unknown process",
                    )
                for spec in PROCESS_PARAM_SPECS[process]:
                    self._register(component="type:snowpack", spec=spec)

            # Snow sublimation (snow container to the atmosphere)
            sublimation = options.get("snow_sublimation_process")
            if sublimation is not None:
                if sublimation not in PROCESS_PARAM_SPECS:
                    raise ConfigurationError(
                        f"The snow_sublimation_process option {sublimation} is "
                        f"not recognised.",
                        item_name="snow_sublimation_process",
                        item_value=sublimation,
                        reason="Unknown process",
                    )
                for spec in PROCESS_PARAM_SPECS[sublimation]:
                    self._register(component="type:snowpack", spec=spec)

            # Snow/ice transformation
            algo = options.get("snow_ice_transformation")
            if algo is not None:
                if algo not in PROCESS_PARAM_SPECS:
                    raise ConfigurationError(
                        f"The snow/ice transformation option {algo} is not recognised.",
                        item_name="snow_ice_transformation",
                        item_value=algo,
                        reason="Unknown transformation algorithm",
                    )
                glacier_names = [
                    cover_name
                    for cover_type, cover_name in zip(
                        land_cover_types, land_cover_names
                    )
                    if cover_type == "glacier"
                ]
                for i, cover_name in enumerate(glacier_names):
                    # Dynamic aliases per glacier snowpack
                    alias_map: dict[str, list[str]] = {}
                    multi = len(glacier_names) > 1
                    c_name_s = cover_name.replace("-", "_")
                    if algo == "transform:snow_ice_constant":
                        if multi:
                            a_rate = [
                                f"snow_ice_rate_{c_name_s}",
                                f"snow_ice_rate_{i}",
                            ]
                        else:
                            a_rate = ["snow_ice_rate"]
                        alias_map = {
                            "snow_ice_transformation_rate": a_rate,
                        }
                    elif algo == "transform:snow_ice_swat":
                        if multi:
                            a_coeff = [
                                f"snow_ice_basal_acc_coeff_{c_name_s}",
                                f"snow_ice_basal_acc_coeff_{i}",
                            ]
                        else:
                            a_coeff = ["snow_ice_basal_acc_coeff"]
                        alias_map = {
                            "snow_ice_transformation_basal_acc_coeff": a_coeff,
                            "north_hemisphere": ["north_hemisphere"],
                        }
                    for spec in PROCESS_PARAM_SPECS[algo]:
                        self._register(
                            component=f"{cover_name}_snowpack",
                            spec=spec,
                            aliases=alias_map.get(spec.name, []),
                        )

            # Snow redistribution
            if "snow_redistribution" in options:
                red = options["snow_redistribution"]
                if red is None:
                    return
                if red in PROCESS_PARAM_SPECS:
                    multi = len(land_cover_names) > 1
                    for spec in PROCESS_PARAM_SPECS[red]:
                        if spec.name == "snow_holding_capacity":
                            # The snow holding capacity is land-cover specific
                            # (vegetation / surface roughness), so register it per
                            # snowpack with a per-cover alias suffix.
                            for cover_name in land_cover_names:
                                suffix = f"_{cover_name}" if multi else ""
                                self._register(
                                    component=f"{cover_name}_snowpack",
                                    spec=spec,
                                    alias_suffix=suffix,
                                )
                        else:
                            self._register(
                                component="type:snowpack",
                                spec=spec,
                                aliases=spec.aliases or [],
                            )
                elif red is not None:
                    raise ConfigurationError(
                        f"The snow redistribution option {red} is not recognised.",
                        item_name="snow_redistribution",
                        item_value=red,
                        reason="Unknown option",
                    )

    @staticmethod
    def _check_min_max_consistency(
        min_val: float | None, max_val: float | None
    ) -> None:
        """
        Validate that minimum value is less than maximum value.

        Parameters
        ----------
        min_val
            Minimum value to check, or None.
        max_val
            Maximum value to check, or None.

        Raises
        ------
        ValueError
            If min_val >= max_val (when both are provided).
        """
        if min_val is None or max_val is None:
            return

        if not isinstance(min_val, list) and not isinstance(max_val, list):
            if max_val < min_val:
                raise ConfigurationError(
                    f"The provided min value ({min_val}) is greater "
                    f"than the max value ({max_val}).",
                    reason="Invalid range",
                )
            return

        if not isinstance(min_val, list) or not isinstance(max_val, list):
            raise ConfigurationError(
                "Mixing lists and floats for the definition of min/max values.",
                reason="Inconsistent parameter types",
            )

        if len(min_val) != len(max_val):
            raise ConfigurationError(
                "The length of the min/max lists are not equal.",
                reason="Mismatched array lengths",
            )

        for min_v, max_v in zip(min_val, max_val):
            if max_v < min_v:
                raise ConfigurationError(
                    f"The provided min value ({min_v}) in list is greater "
                    f"than max value ({max_v}).",
                    reason="Invalid range",
                )

    def _check_aliases_uniqueness(self, aliases: list[str] | None) -> None:
        """
        Validate that all aliases are unique across all parameters.

        Parameters
        ----------
        aliases
            List of aliases to check for uniqueness.

        Raises
        ------
        ValueError
            If any alias already exists in the parameters list.
        """
        if aliases is None:
            return

        # Aliases must be unique case-insensitively, as the parameter lookup is
        # case-insensitive.
        existing_aliases = [
            alias.lower()
            for alias in self.parameters.explode("aliases")["aliases"].tolist()
            if isinstance(alias, str)
        ]
        for alias in aliases:
            if alias.lower() in existing_aliases:
                raise ConfigurationError(
                    f'The alias "{alias}" already exists. It must be unique.',
                    item_name=alias,
                    reason="Duplicate alias",
                )

    def _check_value_range(
        self, index: int, key: str, value: float, allow_adapt: bool = False
    ) -> float:
        """
        Validate that a parameter value is within its allowed range.

        Checks if a value falls within the minimum and maximum bounds defined for
        the parameter. Can optionally adapt the value to fit the bounds.

        Parameters
        ----------
        index
            Index of the parameter in the DataFrame.
        key
            Parameter name or alias (for error messages).
        value
            The value to check.
        allow_adapt
            If True, adapt the value to fit within bounds instead of raising error.

        Returns
        -------
        float
            The validated (and possibly adapted) value.

        Raises
        ------
        ValueError
            If value is out of range and allow_adapt is False.
        """
        max_val = self.parameters.loc[index, "max"]
        min_val = self.parameters.loc[index, "min"]

        if not isinstance(min_val, list):
            if max_val is not None and value > max_val:
                if allow_adapt:
                    return max_val
                raise ConfigurationError(
                    f'The value {value} for the parameter "{key}" is '
                    f"above the maximum threshold ({max_val}).",
                    item_name=key,
                    item_value=value,
                    reason="Value exceeds maximum",
                )
            if min_val is not None and value < min_val:
                if allow_adapt:
                    return min_val
                raise ConfigurationError(
                    f'The value {value} for the parameter "{key}" is '
                    f"below the minimum threshold ({min_val}).",
                    item_name=key,
                    item_value=value,
                    reason="Value below minimum",
                )
        else:
            assert isinstance(max_val, list)
            assert isinstance(value, list)
            for i, (min_v, max_v, val) in enumerate(zip(min_val, max_val, value)):
                if max_v is not None and val > max_v:
                    if allow_adapt:
                        value[i] = max_v
                    else:
                        raise ConfigurationError(
                            f'The value {val} for the parameter "{key}" is '
                            f"above the maximum threshold ({max_v}).",
                            item_name=key,
                            item_value=val,
                            reason="Value exceeds maximum",
                        )
                if min_v is not None and val < min_v:
                    if allow_adapt:
                        value[i] = min_v
                    else:
                        raise ConfigurationError(
                            f'The value {val} for the parameter "{key}" is '
                            f"below the minimum threshold ({min_v}).",
                            item_name=key,
                            item_value=val,
                            reason="Value below minimum",
                        )

        return value

    def _get_transform(self, index: int | Hashable) -> ParameterTransform | None:
        """
        Return the transform attached to a parameter, or None.

        The ``transform`` cell may be missing (filled as NaN by concat for rows that
        omit the key, e.g. data parameters) rather than None, so membership is checked
        by type rather than identity.

        Parameters
        ----------
        index
            Index of the parameter in the DataFrame.

        Returns
        -------
        ParameterTransform | None
            The transform if one is set, otherwise None.
        """
        transform = self.parameters.loc[index, "transform"]
        return transform if isinstance(transform, ParameterTransform) else None

    def _get_parameter_index(
        self, name: str, raise_exception: bool = True
    ) -> int | Hashable | None:
        """
        Get the index of a parameter by name or alias.

        Parameters
        ----------
        name
            The parameter name or alias.
        raise_exception
            If True, raise ConfigurationError if parameter not found. If False,
            return None.

        Returns
        -------
        int | Hashable | None
            The index of the parameter in the DataFrame, or None if not found and
            raise_exception is False.

        Raises
        ------
        ConfigurationError
            If the parameter is not found and raise_exception is True.
        """
        # The matching is case-insensitive (e.g. 'PERC' and 'perc' are equivalent).
        name_lower = name.lower()
        for index, row in self.parameters.iterrows():
            if (
                row["aliases"] is not None
                and name_lower in (alias.lower() for alias in row["aliases"])
                or name_lower == (row["component"] + ":" + row["name"]).lower()
            ):
                return index

        if raise_exception:
            raise ConfigurationError(
                f'The parameter "{name}" was not found.',
                item_name=name,
                reason="Parameter not found",
            )

        return None
