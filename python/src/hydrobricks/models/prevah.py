from __future__ import annotations

import logging
from typing import Any

from hydrobricks._exceptions import ConfigurationError, ModelError
from hydrobricks.models.model import Model
from hydrobricks.modules.glacier import GlacierModule

logger = logging.getLogger(__name__)


class Prevah(Model):
    """PREVAH hydrological model (Viviroli et al., 2009; Gurtz et al., 1999).

    PREVAH (Precipitation-Runoff-EVApotranspiration HRU model) is an HBV-type
    conceptual model developed for mountainous (Alpine) catchments. Each hydro
    unit splits precipitation into rain and snow (linear transition); snow melts
    by a degree-day routine with a seasonally varying melt factor, liquid water
    retention and refreezing. The incoming water is split by the HBV beta
    function between the soil moisture storage (plant-available water, ET
    limited by the CU fraction) and the upper zone (SUZ), which produces:

      - surface runoff above a storage threshold: Q0 = k0 × (SUZ − SGRLUZ),
      - interflow: Q1 = k1 × SUZ,
      - a percolation gated by the soil moisture state (PREVAH's SSM-dependent
        percolation): PERC = cperc × clamp((SM/FC − cu)/(1 − cu), 0, 1).

    The groundwater follows the SLOWCOMP scheme (Schwarze et al., 1999): a fast
    baseflow store SLZ1 (capacity SLZ1MAX, factor k_gw1) is filled first; its
    overflow splits 8/9 into the slow store SLZ2 (k_gw2) and 1/9 into the very
    slow store SLZ3 (k_gw3; PREVAH ties its storage time to 9× that of SLZ1).
    Baseflow is the sum of the three outflows. As in the PREVAH core, there is
    no channel routing: the catchment discharge aggregates the unit outflows.

    On glacierized areas the PREVAH glacier module distinguishes ice and firn
    covers (split at the equilibrium line in preprocessing), melting only when
    snow-free and draining through linear reservoirs; the firn melt reaches the
    groundwater (SLZ1). See
    :class:`~hydrobricks.modules.glacier.PrevahGlacier`.

    The model is integrated by the ODE solver at a daily step, whereas the
    original PREVAH integrates the runoff cascade with 6 sub-steps per day and
    exact-exponential reservoirs: very fast storage times (below ~24 h) are
    therefore approximated. The response factors [1/d] relate to PREVAH's
    storage times in hours as k = 24 / K_h (e.g. K0H = 20 h → k0 = 1.2 1/d).

    Parameters (literature names as aliases)
    ----------------------------------------
    prec_t_start, prec_t_end : float
        Rain/snow transition bounds [°C] (PREVAH: TGR − TTRANS and TGR + TTRANS).
    rfcf, sfcf : float
        Rain and snow correction factors [-] (PREVAH cf_rain/cf_snow).
    a_snow_min (crmfmin), a_snow_max (crmfmax) : float
        Winter minimum and summer maximum of the seasonal snow melt degree-day
        factor [mm/d/°C] (with the default 'melt:degree_day_seasonal').
    melt_t_snow : float
        Snow melting temperature [°C] (PREVAH T0).
    cwh : float
        Snowpack liquid water holding capacity, fraction of the SWE [-].
    cfr : float
        Refreezing coefficient [-] (PREVAH CRFR).
    sublimation_pet_factor : float
        Fraction of the PET applied as snow sublimation [-] (PREVAH computes the
        snow evaporation from the PET).
    fc : float
        Soil moisture storage capacity (plant-available field capacity) [mm].
    beta : float
        Shape coefficient of the beta-function recharge [-] (PREVAH CBETA).
    lp (cu) : float
        Soil moisture fraction above which ET reaches the potential rate [-]
        (PREVAH CU).
    k0 : float
        Surface runoff response factor [1/d] (= 24 / K0H).
    sgrluz : float
        Upper zone storage threshold for surface runoff [mm] (PREVAH SGRLUZ).
    k1 : float
        Interflow response factor [1/d] (= 24 / K1H).
    cperc : float
        Maximum percolation rate [mm/d] (PREVAH CPERC).
    cu_perc : float
        Soil moisture fraction below which percolation stops [-] (PREVAH uses the
        same CU for the ET limit and the percolation gating; fix both to the same
        value for strict PREVAH behavior).
    slz1max : float
        Capacity of the fast baseflow store SLZ1 [mm] (PREVAH SLZ1MAX).
    k_gw1, k_gw2, k_gw3 : float
        Response factors of the three groundwater stores [1/d] (= 24 / CG1H,
        24 / K2H and 24 / CG3H; PREVAH convention: CG3H = 9 × CG1H).
    k_snow, k_ice, k_firn : float
        Response factors of the glacier snowmelt, ice melt and firn melt
        reservoirs [1/d] (= 24 / KICSH, 24 / KICEH, 24 / KICFH).
    wet_fraction : float
        Fraction of a ``wetland`` cover's input routed directly to the
        groundwater store SLZ1 [-] (PREVAH's wet-surface fraction: 0.7 for
        wetlands, 0.9 for open water); the rest goes through the usual
        beta-function soil routine.
    a_ice_min, a_ice_max : float
        Seasonal ice melt degree-day factors [mm/d/°C] (with the default melt
        process; with 'melt:temperature_index', PREVAH's Hock melt
        (CICEMF + CAICE·R_pot)·T, the parameters are melt_factor and r_ice).
    ic : float
        Forest canopy interception capacity [mm] (with forest_interception).

    Options
    -------
    snow_melt_process : str
        Snowmelt method (default: 'melt:degree_day_seasonal', PREVAH's seasonal
        sine between CRMFMIN and CRMFMAX). 'melt:temperature_index' gives
        PREVAH's Hock radiation-corrected melt (requires the potential clear-sky
        radiation forcing); it is incompatible with the degree-day refreezing
        (set snow_refreezing_process=None).
    snow_water_retention_process : str or None
        Outflow process of the snowpack liquid water storage (default:
        'outflow:snow_holding', the CWH holding capacity).
    snow_refreezing_process : str or None
        Refreezing process of the retained liquid water (default:
        'refreeze:degree_day', the CRFR coefficient; uses the current, seasonal
        degree-day factor). Requires a degree-day snow melt process.
    rain_to_snowpack : bool
        Route the rain to the snowpack liquid water storage (default: True, as
        in the PREVAH snow routine).
    snow_sublimation_process : str or None
        Snow sublimation process (default: 'sublimation:pet', as PREVAH derives
        the snow evaporation from the PET).
    snow_rain_process : str or None
        Rain/snow partitioning method (default: None, i.e. 'snow_rain:linear',
        PREVAH's linear transition over TGR ± TTRANS).
    snow_redistribution : str or None
        Optional snow redistribution process (e.g. 'transport:snow_slide').
    forest_interception : bool
        Add a canopy interception store on each ``forest`` land cover (default:
        True; PREVAH's interception module, simplified to a threshold store).
    glacier_infinite_storage : bool
        Treat the glacier ice as an infinite storage (default: True).
    glacier_module : str
        Glacier formulation to plug in (default: 'prevah'; see
        :class:`~hydrobricks.modules.glacier.PrevahGlacier`).
    firn_to_groundwater : bool
        Route the firn melt reservoir into the groundwater store SLZ1 (default:
        True, as in PREVAH); False routes it to the outlet.

    Land covers
    -----------
    Besides the soil-bearing ``open`` and ``forest`` covers (the latter with an
    optional canopy) and the ``glacier`` covers (ice/firn), a ``wetland`` cover
    implements PREVAH's wet-surface behavior: a fraction of its input
    (``wet_fraction``, PREVAH: 0.7 for wetlands, 0.9 for open water) recharges
    the groundwater store SLZ1 directly, the rest passing through the soil
    routine.

    Deviations from the original PREVAH
    -----------------------------------
    - Continuous ODE integration instead of 6 explicit sub-steps per day.
    - The canopy interception fills as a threshold store (no Menzel asymptotic
      filling) with a constant (not monthly) capacity.
    - The snowpack liquid water in excess of CWH·SWE drains within the step
      (no CEXLIQ release exponent).
    - No dynamic contributing-area (soil-topographic-index) surface runoff
      store; the SGRLUZ threshold carries the surface runoff response.
    - Soil ET is not suppressed under snow.
    - PET is computed in preprocessing (e.g. ``forcing.compute_pet``, incl.
      Hamon as in xPREVAH) instead of in-model.
    """

    def __init__(self, name: str = "prevah", **kwargs: Any) -> None:
        super().__init__(name=name, **kwargs)

        # Default options
        self.options["snow_melt_process"] = "melt:degree_day_seasonal"
        self.options["snow_water_retention_process"] = "outflow:snow_holding"
        self.options["snow_refreezing_process"] = "refreeze:degree_day"
        self.options["rain_to_snowpack"] = True
        self.options["snow_rain_process"] = None
        self.options["snow_redistribution"] = None
        self.options["snow_sublimation_process"] = "sublimation:pet"
        self.options["forest_interception"] = True
        self.options["glacier_infinite_storage"] = True
        self.options["glacier_module"] = "prevah"
        self.options["firn_to_groundwater"] = True
        self.allowed_land_cover_types = ["open", "forest", "wetland", "glacier"]

        self._set_options(kwargs)

        try:
            self._define_structure()
            self._generate_structure()
            self._define_parameter_aliases()
            self._define_parameter_constraints()
            self._define_parameter_transforms()

        except RuntimeError as err:
            raise ModelError(f"Prevah model initialization raised an exception: {err}")

    def _define_structure(self) -> None:
        """Define the PREVAH model structure.

        The brick declaration order matters (the solver applies the bricks in
        order, so every brick-to-brick flux must flow toward a later brick):
        soil covers → glacier bricks (their firn reservoir feeds the groundwater)
        → soil moisture → upper zone → SLOWCOMP groundwater (slz1 → split →
        slz2/slz3). PREVAH has no capillary flux, so the soil moisture store
        receives water only from the covers.

        A single soil moisture store is shared by the soil covers (a PREVAH
        hydrotope carries one soil), which also serves as the gate brick of the
        soil-moisture-dependent percolation.
        """
        self._glacier_cover_names = [
            name
            for name, cover_type in zip(self.land_cover_names, self.land_cover_types)
            if cover_type == "glacier"
        ]
        self._wetland_cover_names = [
            name
            for name, cover_type in zip(self.land_cover_names, self.land_cover_types)
            if cover_type == "wetland"
        ]
        soil_cover_names = [
            name
            for name, cover_type in zip(self.land_cover_names, self.land_cover_types)
            if cover_type not in ("glacier", "wetland")
        ]
        if not soil_cover_names:
            raise ConfigurationError(
                "The PREVAH model requires at least one soil-bearing land cover "
                "(open or forest).",
                item_name="land_cover_types",
                reason="Only glacier/wetland covers provided",
            )
        multi_cover = len(soil_cover_names) + len(self._wetland_cover_names) > 1

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
                        "target": "soil_moisture",
                    },
                    "recharge": {"kind": "outflow:rest", "target": "upper_zone"},
                },
            }
            if multi_cover:
                brick["alias_suffix"] = f"_{cover_name}"
            self.structure[cover_name] = brick

        # Wetland covers (PREVAH wet-surface): a fixed fraction of the input
        # (rain + snowpack outflow) recharges the groundwater store directly
        # (PREVAH's ri_w on wet surfaces); the rest goes through the usual
        # beta-function soil routine via a per-cover pass-through brick.
        for cover_name in self._wetland_cover_names:
            self.structure[cover_name] = {
                "attach_to": "hydro_unit",
                "kind": "land_cover",
                "processes": {
                    "split": {
                        "kind": "outflow:split",
                        "targets": ["slz1", f"{cover_name}_dry"],
                    },
                },
            }
            dry = {
                "attach_to": "hydro_unit",
                "kind": "storage",
                "processes": {
                    "infiltration": {
                        "kind": "infiltration:hbv",
                        "target": "soil_moisture",
                    },
                    "recharge": {"kind": "outflow:rest", "target": "upper_zone"},
                },
            }
            if multi_cover:
                dry["alias_suffix"] = f"_{cover_name}"
            self.structure[f"{cover_name}_dry"] = dry

        # Glacier bricks (pluggable module). Called before the groundwater stores so
        # the firn melt reservoir can feed slz1 with a forward flux.
        self._glacier_module = GlacierModule.get_module(self.options["glacier_module"])
        self._glacier_module.add_bricks(
            self.structure,
            self._glacier_cover_names,
            melt_process=self.options["snow_melt_process"],
            options=self.options,
        )

        # Soil moisture storage (plant-available field capacity FC): ET limited by
        # the CU fraction (et:hbv with lp = CU). The overflow is a numerical safety
        # only (the infiltration vanishes at FC).
        self.structure["soil_moisture"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "parameters": {"capacity": 250},
            "processes": {
                "et": {"kind": "et:hbv"},
                "overflow": {"kind": "overflow", "target": "upper_zone"},
            },
        }

        # Upper zone (SUZ): threshold surface runoff, interflow and the
        # soil-moisture-gated percolation to the groundwater.
        self.structure["upper_zone"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {
                "q0": {"kind": "outflow:linear_threshold", "target": "outlet"},
                "q1": {"kind": "outflow:linear", "target": "outlet"},
                "percolation": {
                    "kind": "percolation:prevah",
                    "target": "slz1",
                    "gate": "soil_moisture",
                },
            },
        }

        # SLOWCOMP groundwater (Schwarze et al., 1999): the fast store SLZ1 is
        # filled first (capacity SLZ1MAX); its overflow splits 8/9 : 1/9 into the
        # slow stores SLZ2 and SLZ3.
        self.structure["slz1"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "parameters": {"capacity": 30},
            "processes": {
                "baseflow1": {"kind": "outflow:linear", "target": "outlet"},
                "overflow": {"kind": "overflow", "target": "slz_split"},
            },
        }
        self.structure["slz_split"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {
                "split": {"kind": "outflow:split", "targets": ["slz2", "slz3"]},
            },
        }
        self.structure["slz2"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {
                "baseflow2": {"kind": "outflow:linear", "target": "outlet"},
            },
        }
        self.structure["slz3"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {
                "baseflow3": {"kind": "outflow:linear", "target": "outlet"},
            },
        }

    def _define_structure_variants(
        self,
    ) -> list[tuple[list[str], list[str], dict[str, Any]]]:
        """Make the glacier-free structure the base, adding a with-glacier variant.

        As in Socont/HBV: glacier-free units use the base structure (no glacier
        brick at all), glacierized units the with-glacier variant. The split is
        handled by the shared glacier machinery.
        """
        return self._split_glacier_variants(
            self.land_cover_names, self.land_cover_types, self.structure
        )

    def _define_parameter_aliases(self) -> None:
        """Define PREVAH parameter aliases (literature names).

        The process parameter specs already provide beta (CBETA), lp (the CU
        ET limit), cwh, cfr, cperc, cu_perc, the seasonal melt factors
        (a_snow_min/crmfmin, a_snow_max/crmfmax) and the sublimation factor;
        the glacier reservoir factors come from the glacier module.
        """
        self.parameter_aliases = {
            "soil_moisture:capacity": ["fc"],
            "soil_moisture:lp": ["cu"],
            "upper_zone:response_factor_threshold": ["k0"],
            "upper_zone:threshold": ["sgrluz"],
            "upper_zone:response_factor": ["k1"],
            "slz1:capacity": ["slz1max"],
            "slz1:response_factor": ["k_gw1"],
            "slz2:response_factor": ["k_gw2"],
            "slz3:response_factor": ["k_gw3"],
        }
        single_wetland = len(self._wetland_cover_names) == 1
        for cover_name in self._wetland_cover_names:
            alias = "wet_fraction" if single_wetland else f"wet_fraction_{cover_name}"
            self.parameter_aliases[f"{cover_name}:split_fraction"] = [alias]
        if self._glacier_module is not None:
            self.parameter_aliases.update(
                self._glacier_module.parameter_aliases(self._glacier_cover_names)
            )

    def _define_parameter_constraints(self) -> None:
        """Define parameter constraints for the PREVAH model.

        The response cascade must slow down with depth: surface runoff faster
        than interflow, interflow faster than the fast baseflow, and the three
        groundwater stores in decreasing order (PREVAH convention:
        CG3H = 9 × CG1H, i.e. k_gw3 ≈ k_gw1 / 9).
        """
        self.parameter_constraints = [
            ["k1", "<", "k0"],
            ["k_gw1", "<", "k1"],
            ["k_gw2", "<", "k_gw1"],
            ["k_gw3", "<", "k_gw2"],
        ]

    def _set_specific_options(self, kwargs: dict[str, Any]) -> None:
        """Validate PREVAH-specific option combinations."""
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
            melt = self.options.get("snow_melt_process")
            if melt not in ("melt:degree_day", "melt:degree_day_seasonal"):
                raise ConfigurationError(
                    "The refreeze:degree_day process requires a degree-day snow "
                    "melt process (melt:degree_day or melt:degree_day_seasonal).",
                    item_name="snow_melt_process",
                    item_value=melt,
                    reason="Incompatible option",
                )
