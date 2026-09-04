from __future__ import annotations

import logging
from typing import Any

from hydrobricks._exceptions import ConfigurationError, ModelError
from hydrobricks.models.model import Model
from hydrobricks.modules.glacier import GlacierModule

logger = logging.getLogger(__name__)


class PrevahUniBE(Model):
    """PREVAH-UniBE hydrological model (Viviroli et al., 2007; Gurtz et al., 1999).

    PREVAH (Precipitation-Runoff-EVApotranspiration HRU model) is an HBV-type
    conceptual model developed for mountainous (Alpine) catchments. This
    implementation follows the University of Bern lineage of the model (the
    xPREVAH code base), hence the name PREVAH-UniBE. Each hydro
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

    The response factors [1/d] relate to PREVAH's storage times in hours as
    k = 24 / K_h (e.g. K0H = 20 h → k0 = 1.2 1/d). That mapping is exact rather
    than approximate: PREVAH's own storage coefficients are already exponential
    decay factors (the Fortran computes k = 1 − exp(−Δt/K_h) once at
    initialization, then applies it as a plain multiplier), and a linear
    reservoir integrated analytically decays over a day by exactly
    exp(−24 / K_h). This is why the model defaults to the ``analytic_linear``
    solver, which integrates the reservoirs exactly as PREVAH does; the other
    solvers approximate them. The remaining difference is the sub-step
    structure: PREVAH adds the inflow at the start of each of its sub-steps
    (6 per day; 1 h in the gridded version), where the solver spreads it over
    the whole step.

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
        Snow evaporation process (default: 'sublimation:pet', a fixed
        ``sublimation_pet_factor`` of the PET). 'sublimation:prevah' evaporates
        snow at the albedo-reduced potential rate (PET (1 - albedo)/0.8 with the
        snowpack's age-dependent albedo), as PREVAH does — the faithful,
        parameter-free snow evaporation.
    snow_rain_process : str or None
        Rain/snow partitioning method (default: None, i.e. 'snow_rain:linear',
        PREVAH's linear transition over TGR ± TTRANS).
    snow_redistribution : str or None
        Optional snow redistribution process (e.g. 'transport:snow_slide').
    share_soil : bool
        Share a single soil moisture store across the soil-bearing land covers
        (default: True). A PREVAH hydrotope carries one land use and one soil, so a
        shared store is the right model when each hydro unit is dominated by one
        cover. Set it to False when a unit mixes covers and you want PREVAH's
        per-land-use soil parameterization: each cover then gets its own store with
        its own ``fc_<cover>`` and ``lp_<cover>``, and the percolation is gated by
        their area-weighted mean saturation. Note that a per-cover store is fed by
        its land cover, whose outgoing fluxes already carry the cover area fraction,
        so ``fc_<cover>`` is expressed over the whole hydro unit rather than over the
        cover itself (as for the per-class soils of
        :class:`~hydrobricks.models.hbv.HBV`): the capacities of the covers add up to
        the unit's soil storage.
    forest_interception : bool
        Add a canopy interception store on each ``forest`` land cover (default:
        True). Superseded by ``interception_covers`` when that option is set.
    interception_covers : list[str] | None
        Names of the land covers to equip with a canopy interception store
        (default None: the forest covers). The original PREVAH applies its
        interception module (Menzel filling, evaporation at et_pot * veg_cov) to
        EVERY vegetated cover — pass all vegetated cover names to reproduce that.
        With canopy_et_process='et:open_water_prevah', each canopy gets an
        ``et_factor`` parameter (alias ``canopy_et_factor[_<cover>]``) for the
        monthly PREVAH veg_cov fraction (set via set_monthly_values).
    canopy_interception_process : str
        Throughfall process of the forest canopy (default: 'interception:menzel',
        PREVAH's Menzel (1997) asymptotic filling). Use 'outflow:threshold' for a
        simpler fill-then-spill store.
    soil_et_process : str
        Soil evapotranspiration process (default: 'et:hbv', the HBV limitation).
        'et:prevah' additionally applies PREVAH's snow-albedo reduction of the
        potential rate ((1 - albedo)/0.8, from the unit's snow-covered fraction),
        which suppresses the soil ET under snow. The snow albedo is age-dependent
        (0.4 + 0.45 exp(-0.15 age), ~0.85 fresh to 0.4 old); ``albedo_land``
        (default 0.2) is the snow-free ground albedo (neutral).
    canopy_et_process : str
        Canopy evaporation process (default: 'et:open_water', the potential
        rate). 'et:open_water_prevah' additionally applies the same age-dependent
        snow-albedo reduction to the interception ET.
    wet_et_from_groundwater : bool
        Add PREVAH's wet-surface evaporation (default: False). When True, the
        SLZ1 groundwater store evaporates at ``et_pot * et_factor`` (process
        'et:open_water_prevah' named ``wet_et``; factor alias ``ow_et_factor``).
        Set the factor per unit via a spatial parameter (PREVAH wet_surface:
        0.7 on wetland, 0.9 on water, 0 elsewhere).
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

    Faithful configuration
    ----------------------
    The model defaults to the ``"analytic_linear"`` solver, which integrates the
    linear reservoirs exactly as PREVAH does; pass ``solver=...`` to override it.
    The remaining defaults favour a simple, robust model. To reproduce the
    original as closely as possible, add::

        soil_et_process='et:prevah'
        canopy_et_process='et:open_water_prevah'
        snow_sublimation_process='sublimation:prevah'
        snow_water_retention_process='outflow:snow_holding_prevah'
        snow_refreezing_process='refreeze:degree_day_seasonal'
        interception_covers=[<every vegetated cover>]
        wet_et_from_groundwater=True

    with a vapour-density Hamon PET (``forcing.compute_pet(method=
    "Hamon_vapor_density")``) and the monthly vegetation tables set through
    ``ParameterSet.set_monthly_values`` (``ic`` = si_max × veg_cov,
    ``canopy_et_factor`` = veg_cov). With ``snow_melt_process=
    'melt:temperature_index'`` and a potential clear-sky radiation forcing, the
    snow melt follows PREVAH's radiation-corrected (Hock) formulation.

    Deviations from the original PREVAH
    -----------------------------------
    - The runoff cascade is integrated over the whole time step instead of
      PREVAH's 6 explicit sub-steps per day. The default ``"analytic_linear"``
      solver integrates the linear reservoirs exactly, as PREVAH does, but the
      inflow is spread over the step rather than added at the start of each
      sub-step.
    - No dynamic contributing-area (soil-topographic-index) surface runoff
      store; the SGRLUZ threshold carries the surface runoff response. That
      store (SSZ/CRSZ) is present in the PREVAH code lineages (both xPREVAH and
      the gridded FORHYCS) but in none of the published model descriptions, and
      both codes disable it for catchments above 100 km².
    - No runoff concentration or flood routing (PREVAH's single linear storage
      and translation elements): the unit outflows aggregate directly at the
      outlet, as in the PREVAH model core.
    - The glacier melt reservoirs carry no translation (lag) element; their
      translation times are sub-daily.
    - No karst outflow (the optional fourth upper-zone outflow Q3 = k3 · SUZ of
      the gridded PREVAH). It is a plain linear reservoir outflow, so it can be
      added to the upper zone through a custom structure if needed.
    - The percolation rate is not scaled by the soil hydraulic conductivity
      (PREVAH's KWPER factor).
    - PET is computed in preprocessing (e.g. ``forcing.compute_pet``, incl. the
      vapour-density Hamon used by PREVAH) instead of in-model.
    """

    def __init__(self, name: str = "prevah_unibe", **kwargs: Any) -> None:
        # PREVAH integrates its reservoirs analytically (an exact exponential decay
        # per sub-step), so the analytic solver is the faithful default here; any
        # other solver can still be requested explicitly.
        kwargs.setdefault("solver", "analytic_linear")
        super().__init__(name=name, **kwargs)

        # Default options
        self.options["snow_melt_process"] = "melt:degree_day_seasonal"
        self.options["snow_water_retention_process"] = "outflow:snow_holding"
        self.options["snow_refreezing_process"] = "refreeze:degree_day"
        self.options["rain_to_snowpack"] = True
        self.options["snow_rain_process"] = None
        self.options["snow_redistribution"] = None
        self.options["snow_sublimation_process"] = "sublimation:pet"
        self.options["share_soil"] = True
        self.options["forest_interception"] = True
        self.options["interception_covers"] = None
        self.options["canopy_interception_process"] = "interception:menzel"
        self.options["soil_et_process"] = "et:hbv"
        self.options["canopy_et_process"] = "et:open_water"
        self.options["wet_et_from_groundwater"] = False
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
            raise ModelError(
                f"PREVAH-UniBE model initialization raised an exception: {err}"
            )

    def _define_structure(self) -> None:
        """Define the PREVAH model structure.

        The brick declaration order matters (the solver applies the bricks in
        order, so every brick-to-brick flux must flow toward a later brick):
        soil covers → glacier bricks (their firn reservoir feeds the groundwater)
        → soil moisture → upper zone → SLOWCOMP groundwater (slz1 → split →
        slz2/slz3). PREVAH has no capillary flux, so the soil moisture store
        receives water only from the covers.

        A single soil moisture store is shared by the soil covers by default (a
        PREVAH hydrotope carries one land use and one soil); ``share_soil=False``
        gives each cover its own store and its own field capacity, as PREVAH
        parameterizes the soil per land-use class. The soil store(s) also gate the
        soil-moisture-dependent percolation: with several, the percolation reads
        their area-weighted mean saturation.
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

        # Soil naming: a single shared store by default (the PREVAH hydrotope), or one
        # soil moisture store per soil-bearing cover when share_soil is disabled. The
        # wetland covers route their dry fraction through their own pass-through brick,
        # which feeds the soil of that cover.
        # Brick feeding a soil store -> the land cover that owns it. A wetland feeds
        # its soil through its own pass-through brick, but the store (and its
        # parameters) belong to the cover itself.
        soil_sources = {name: name for name in soil_cover_names}
        soil_sources.update({f"{n}_dry": n for n in self._wetland_cover_names})
        self._shared_soil = (
            bool(self.options.get("share_soil")) or len(soil_sources) == 1
        )
        if self._shared_soil:
            self._soil_names = {src: "soil_moisture" for src in soil_sources}
        else:
            self._soil_names = {
                src: f"{cover}_soil_moisture" for src, cover in soil_sources.items()
            }
        # Soil store -> the cover naming its parameters (fc_<cover>, lp_<cover>).
        self._soil_covers = {
            self._soil_names[src]: cover for src, cover in soil_sources.items()
        }

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
                        "target": self._soil_names[cover_name],
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
                        "target": self._soil_names[f"{cover_name}_dry"],
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
        # the CU fraction (lp = CU). The overflow is a numerical safety only (the
        # infiltration vanishes at FC). With soil_et_process='et:prevah' the potential
        # rate additionally carries the PREVAH snow-albedo reduction (1 - albedo)/0.8,
        # driven by the unit's snow-covered fraction (ET-under-snow suppression).
        soil_et = self.options["soil_et_process"]
        if soil_et not in ("et:hbv", "et:prevah"):
            raise ConfigurationError(
                f"Unknown soil ET process: '{soil_et}'. "
                "Expected 'et:hbv' or 'et:prevah'.",
                item_name="soil_et_process",
                item_value=soil_et,
                reason="Unknown process type",
            )
        canopy_et = self.options["canopy_et_process"]
        if canopy_et not in ("et:open_water", "et:open_water_prevah"):
            raise ConfigurationError(
                f"Unknown canopy ET process: '{canopy_et}'. "
                "Expected 'et:open_water' or 'et:open_water_prevah'.",
                item_name="canopy_et_process",
                item_value=canopy_et,
                reason="Unknown process type",
            )
        for soil_name, cover_name in self._soil_covers.items():
            if soil_name in self.structure:
                continue  # shared store already declared
            soil = {
                "attach_to": "hydro_unit",
                "kind": "storage",
                "parameters": {"capacity": 250},
                "processes": {
                    "et": {"kind": soil_et},
                    "overflow": {"kind": "overflow", "target": "upper_zone"},
                },
            }
            if not self._shared_soil and multi_cover:
                soil["alias_suffix"] = f"_{cover_name}"
            self.structure[soil_name] = soil

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
                    # With one soil store per cover, the percolation is gated by their
                    # area-weighted mean saturation (the contents already carry the
                    # cover area fractions).
                    "gate": self._gate_bricks(),
                },
            },
        }

        # SLOWCOMP groundwater (Schwarze et al., 1999): the fast store SLZ1 is
        # filled first (capacity SLZ1MAX); its overflow splits 8/9 : 1/9 into the
        # slow stores SLZ2 and SLZ3.
        # The fast baseflow store SLZ1 uses the PREVAH SLOWCOMP fill: it fills
        # asymptotically toward its maximum (slz1max) with the baseflow time constant
        # and overflows the excess inflow to the slow stores (outflow:slowcomp), rather
        # than a bucket that spills only when full. This diverts the high-percolation
        # (snow-melt) events to the slow stores, sustaining the recession. The store
        # therefore carries no hard capacity (the asymptotic fill bounds it).
        self.structure["slz1"] = {
            "attach_to": "hydro_unit",
            "kind": "storage",
            "processes": {
                "baseflow1": {"kind": "outflow:linear", "target": "outlet"},
                "overflow": {"kind": "outflow:slowcomp", "target": "slz_split"},
            },
        }
        # PREVAH wet-surface evaporation (EWET = wet_surface * et_pot, drawn from the
        # SLOWCOMP stores; s_abfg6eth). Implemented on SLZ1 with the albedo-aware
        # open-water ET; the wet fraction goes in the process' et_factor (alias
        # ow_et_factor), typically per unit via a spatial parameter (0.7 on wetland
        # units, 0.9 on water, 0 elsewhere).
        if self.options["wet_et_from_groundwater"]:
            self.structure["slz1"]["processes"]["wet_et"] = {
                "kind": "et:open_water_prevah",
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

    def _gate_bricks(self) -> str | list[str]:
        """The soil moisture store(s) gating the percolation.

        A single name with a shared store (the usual PREVAH hydrotope), otherwise the
        list of the per-cover stores, whose saturations the process averages.
        """
        names = list(dict.fromkeys(self._soil_names.values()))

        return names[0] if len(names) == 1 else names

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
            "upper_zone:response_factor_threshold": ["k0"],
            "upper_zone:threshold": ["sgrluz"],
            "upper_zone:response_factor": ["k1"],
            "slz1:response_factor": ["k_gw1"],
            "slz2:response_factor": ["k_gw2"],
            "slz3:response_factor": ["k_gw3"],
        }
        if self._shared_soil:
            self.parameter_aliases["soil_moisture:capacity"] = ["fc"]
            self.parameter_aliases["soil_moisture:lp"] = ["cu"]
        else:
            for soil_name, cover_name in self._soil_covers.items():
                self.parameter_aliases[f"{soil_name}:capacity"] = [f"fc_{cover_name}"]
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
            if refreezing == "refreeze:degree_day" and melt not in (
                "melt:degree_day",
                "melt:degree_day_seasonal",
            ):
                raise ConfigurationError(
                    "The refreeze:degree_day process requires a degree-day snow "
                    "melt process (melt:degree_day or melt:degree_day_seasonal). "
                    "Use 'refreeze:degree_day_seasonal' (own seasonal factor, "
                    "PREVAH) with other melt processes.",
                    item_name="snow_melt_process",
                    item_value=melt,
                    reason="Incompatible option",
                )
