from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any

from hydrobricks._exceptions import ConfigurationError


class GlacierModule(ABC):
    """Pluggable glacier formulation for the hydrological models.

    A glacier module owns everything glacier-specific so that the same formulation
    can be shared across models (e.g. Socont and HBV) and alternative formulations
    swapped in. It contributes:

    - the glacier-related bricks added to the model structure (``add_bricks``),
    - the structure keys that are glacier land covers, so the model can split a
      glacier-free base from a with-glacier variant (``land_cover_keys``),
    - the glacier-specific parameter aliases (``parameter_aliases``).

    The melt parameters (e.g. ``a_ice``) and the ``a_snow < a_ice`` constraint are
    derived generically by ``ParameterSet`` from the ``melt`` process placed on the
    glacier land cover, so a module only needs to expose its own extra aliases (such
    as the response factors of its reservoirs).
    """

    @abstractmethod
    def add_bricks(
        self,
        structure: dict[str, Any],
        glacier_names: list[str],
        *,
        melt_process: str,
        options: dict[str, Any],
    ) -> None:
        """Add the glacier bricks for the given glacier covers into ``structure``.

        Parameters
        ----------
        structure
            The model structure dictionary to add the bricks to (modified in place).
        glacier_names
            The names of the glacier land covers.
        melt_process
            The ice melt process kind (e.g. ``'melt:degree_day'``), usually the
            model's snow melt process.
        options
            The model options (a module reads its own configuration from here, e.g.
            ``glacier_infinite_storage``).
        """

    @abstractmethod
    def land_cover_keys(self, glacier_names: list[str]) -> set[str]:
        """Return the structure keys that are glacier land covers.

        These are excluded from the glacier-free base variant (while any shared,
        catchment-level bricks the module adds, such as sub-basin reservoirs, stay
        in the base so the sub-basin owns and shares them).
        """

    @abstractmethod
    def parameter_aliases(self, glacier_names: list[str]) -> dict[str, list[str]]:
        """Return the glacier-specific parameter aliases (e.g. reservoir factors)."""


class GSM(GlacierModule):
    """Glacier Sub-Model (GSM), as in GSM-SOCONT (Schaefli et al., 2005).

    The glacierized area sends its rain + snowmelt and its ice melt to two
    catchment-level linear reservoirs draining directly to the outlet, bypassing the
    soil routine. Ice melt is suppressed while snow covers the ice
    (``no_melt_when_snow_cover``); the ice is treated as an infinite storage by
    default (``glacier_infinite_storage``). The two reservoirs are shared by all
    glacier covers.
    """

    RAIN_SNOWMELT_STORAGE = "glacier_area_rain_snowmelt_storage"
    ICEMELT_STORAGE = "glacier_area_icemelt_storage"

    def add_bricks(
        self,
        structure: dict[str, Any],
        glacier_names: list[str],
        *,
        melt_process: str,
        options: dict[str, Any],
    ) -> None:
        if not glacier_names:
            return

        infinite_storage = options.get("glacier_infinite_storage", True)
        for cover_name in glacier_names:
            structure[cover_name] = {
                "attach_to": "hydro_unit",
                "kind": "land_cover",
                "parameters": {
                    "no_melt_when_snow_cover": True,
                    "infinite_storage": infinite_storage,
                },
                "processes": {
                    "outflow_rain_snowmelt": {
                        "kind": "outflow:direct",
                        "target": self.RAIN_SNOWMELT_STORAGE,
                        "instantaneous": True,
                    },
                    "melt": {
                        "kind": melt_process,
                        "target": self.ICEMELT_STORAGE,
                        "instantaneous": True,
                    },
                },
            }

        # Catchment-level reservoirs for the glacierized-area contributions.
        structure[self.RAIN_SNOWMELT_STORAGE] = {
            "attach_to": "sub_basin",
            "kind": "storage",
            "processes": {"outflow": {"kind": "outflow:linear", "target": "outlet"}},
        }
        structure[self.ICEMELT_STORAGE] = {
            "attach_to": "sub_basin",
            "kind": "storage",
            "processes": {"outflow": {"kind": "outflow:linear", "target": "outlet"}},
        }

    def land_cover_keys(self, glacier_names: list[str]) -> set[str]:
        # The sub-basin reservoirs are intentionally not listed: they stay in the
        # glacier-free base so the (catchment-level) sub-basin owns and shares them.
        return set(glacier_names)

    def parameter_aliases(self, glacier_names: list[str]) -> dict[str, list[str]]:
        if not glacier_names:
            return {}
        return {
            f"{self.RAIN_SNOWMELT_STORAGE}:response_factor": ["k_snow"],
            f"{self.ICEMELT_STORAGE}:response_factor": ["k_ice"],
        }


# Registry of the available glacier modules, keyed by name (mirroring the string
# process kinds used elsewhere). Users select one with the ``glacier_module`` option.
_GLACIER_MODULES: dict[str, type[GlacierModule]] = {
    "gsm": GSM,
}


def get_glacier_module(module: str | GlacierModule) -> GlacierModule:
    """Resolve a glacier module from a registry name or a module instance.

    Parameters
    ----------
    module
        Either a registered module name (e.g. ``'gsm'``) or a ``GlacierModule``
        instance (for custom user-defined formulations).

    Returns
    -------
    GlacierModule
        The resolved glacier module instance.

    Raises
    ------
    ConfigurationError
        If the name is not a registered glacier module.
    """
    if isinstance(module, GlacierModule):
        return module
    if module in _GLACIER_MODULES:
        return _GLACIER_MODULES[module]()
    raise ConfigurationError(
        f"The glacier module '{module}' is not recognised. "
        f"Available modules: {', '.join(sorted(_GLACIER_MODULES))}.",
        item_name="glacier_module",
        item_value=module,
        reason="Unknown glacier module",
    )
