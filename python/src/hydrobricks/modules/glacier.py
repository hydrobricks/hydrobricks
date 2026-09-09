from __future__ import annotations

from abc import abstractmethod
from typing import Any

from hydrobricks.modules.base import Module


class GlacierModule(Module):
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

    Resolve a glacier module with ``GlacierModule.get_module(name_or_instance)`` and
    register a new one with the ``@GlacierModule.register("name")`` decorator.
    """

    _registry: dict[str, type[GlacierModule]] = {}
    _category = "glacier module"

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


@GlacierModule.register("gsm")
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


@GlacierModule.register("prevah")
class PrevahGlacier(GlacierModule):
    """PREVAH glacier formulation (Gurtz et al., 2003; Zappa et al., 2003).

    As in PREVAH, the glacierized area distinguishes bare ice (below the
    equilibrium line) from firn (above it), each melting only when snow-free
    (``no_melt_when_snow_cover``) and draining through its own linear reservoir:

    - **ice** covers: rain + snowmelt feed a catchment-level snowmelt reservoir
      (PREVAH's KICSH) and the ice melt a catchment-level ice melt reservoir
      (KICEH), both draining to the outlet;
    - **firn** covers (any glacier cover whose name contains ``'firn'``): their
      melt drains through a per-unit firn reservoir (KICFH) into the groundwater
      (the ``slz1`` store, as PREVAH routes the firn melt to the baseflow module);
      set the ``firn_to_groundwater`` option to False to route it to the outlet
      instead. Their rain + snowmelt joins the snowmelt reservoir.

    The split of the glacier area into ice and firn covers (at the ELA) is a
    preprocessing step; without a firn cover the module behaves like the GSM
    module with PREVAH reservoir semantics. The ice is treated as an infinite
    storage by default (``glacier_infinite_storage``). The reservoir response
    factors [1/d] relate to PREVAH's storage times in hours as k = 24 / K_h.
    """

    RAIN_SNOWMELT_STORAGE = "glacier_area_rain_snowmelt_storage"
    ICEMELT_STORAGE = "glacier_area_icemelt_storage"
    FIRNMELT_STORAGE = "glacier_area_firnmelt_storage"

    @staticmethod
    def _is_firn(cover_name: str) -> bool:
        return "firn" in cover_name.lower()

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
        firn_to_groundwater = options.get("firn_to_groundwater", True)
        firn_names = [name for name in glacier_names if self._is_firn(name)]
        ice_names = [name for name in glacier_names if not self._is_firn(name)]

        for cover_name in glacier_names:
            is_firn = self._is_firn(cover_name)
            melt_target = self.FIRNMELT_STORAGE if is_firn else self.ICEMELT_STORAGE
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
                        "target": melt_target,
                        "instantaneous": True,
                    },
                },
            }

        # Per-unit firn reservoir: declared before the groundwater stores (the model
        # adds them later), so its outflow into slz1 flows toward a later brick.
        if firn_names:
            firn_target = "slz1" if firn_to_groundwater else "outlet"
            structure[self.FIRNMELT_STORAGE] = {
                "attach_to": "hydro_unit",
                "kind": "storage",
                "processes": {
                    "outflow": {"kind": "outflow:linear", "target": firn_target}
                },
            }

        # Catchment-level reservoirs for the glacierized-area contributions.
        structure[self.RAIN_SNOWMELT_STORAGE] = {
            "attach_to": "sub_basin",
            "kind": "storage",
            "processes": {"outflow": {"kind": "outflow:linear", "target": "outlet"}},
        }
        if ice_names:
            structure[self.ICEMELT_STORAGE] = {
                "attach_to": "sub_basin",
                "kind": "storage",
                "processes": {
                    "outflow": {"kind": "outflow:linear", "target": "outlet"}
                },
            }

    def land_cover_keys(self, glacier_names: list[str]) -> set[str]:
        # The reservoirs are intentionally not listed: they stay in the glacier-free
        # base (inert for glacier-free units), so the sub-basin owns the shared ones
        # and the per-unit firn reservoir keeps a consistent brick order.
        return set(glacier_names)

    def parameter_aliases(self, glacier_names: list[str]) -> dict[str, list[str]]:
        if not glacier_names:
            return {}
        aliases = {f"{self.RAIN_SNOWMELT_STORAGE}:response_factor": ["k_snow"]}
        if any(not self._is_firn(name) for name in glacier_names):
            aliases[f"{self.ICEMELT_STORAGE}:response_factor"] = ["k_ice"]
        if any(self._is_firn(name) for name in glacier_names):
            aliases[f"{self.FIRNMELT_STORAGE}:response_factor"] = ["k_firn"]
        return aliases
