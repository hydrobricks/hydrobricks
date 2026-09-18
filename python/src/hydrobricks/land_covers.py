"""Land cover types and land-use parameter tables shared across the models.

Two terms are kept apart throughout hydrobricks:

- a **land cover** is part of the model: a cover brick with a name, a type (``open``,
  ``forest``, ``glacier``, ...) and an area fraction in each hydro unit;
- a **land use** (or land-use class) is an entry of an external classification whose
  tables supply parameters, such as PREVAH's 22 classes (``pasture``,
  ``coniferous_forest``, ...). A land cover is parameterized from a land use, and may
  be named after it, but several land uses can share one land cover.
"""

#: Canonical generic cover names (the "open areas" class and its backward-compatible
#: aliases). Treated as interchangeable with ``open`` everywhere.
GENERIC_COVER_ALIASES = frozenset({"open", "ground", "generic", "generic_land_cover"})

#: Land cover types that behave as a generic soil cover (same generic_land_cover
#: brick and soil routine) but keep a distinct identity for labelling, land-cover
#: extraction and per-cover parameters. Accepted by any model that accepts the generic
#: cover, so they are available globally without each model listing them; unlike the
#: generic aliases they are not interchangeable with ``open`` (a model may special-case
#: them, as PREVAH does for ``wetland``). ``urban`` (built-up) and ``rock`` (bare
#: rock / sparsely vegetated terrain) have no impervious routine of their own yet — they
#: are distinct generic soil covers so those areas can be tracked and parameterized
#: separately (e.g. a very low field capacity, as PREVAH uses for built-up and rock).
GENERIC_SOIL_COVER_TYPES = frozenset({"wetland", "urban", "rock"})

#: Exclusive open-water cover: direct precipitation, open-water evaporation, no snow.
#: Handled by the models that support it (currently HBV); not a generic soil cover.
#: NOTE: ``lake`` is intentionally NOT a synonym — it is reserved for a future distinct
#: cover (a lake/reservoir store with its own level-storage-release rules).
WATER_COVER_TYPE = "water"


# ---------------------------------------------------------------------------
# PREVAH's built-in land-use parameterization
#
# The original PREVAH ships a fixed table of land uses. Only six of them carry
# distinct physics -- water, built-up, firn, bare ice, rock and wetlands -- and
# those are the hydrobricks land cover types of the same name. Every other land use
# runs the same snow, soil and runoff routines and differs only through the monthly
# vegetation tables below, so it is an ordinary land cover carrying its own monthly
# parameters. PREVAH's class numbers are not kept: the land uses are named, following
# the hydrobricks conventions where a matching concept exists.
#
# Transcribed from the reference XPREVAH implementation.
# ---------------------------------------------------------------------------

# fmt: off
#: The land uses of PREVAH's built-in parameterization, and the hydrobricks land
#: cover type each one behaves as. Only water, urban, glacier,
#: rock and wetland carry distinct physics in PREVAH; every other land use is an
#: ordinary soil-bearing cover that differs only through the monthly tables below,
#: so it needs no dedicated cover type.
PREVAH_LAND_USE_COVER_TYPES: dict[str, str] = {
    "water": "water",
    "urban": "urban",
    "coniferous_forest": "forest",
    "deciduous_forest": "forest",
    "mixed_forest": "forest",
    "cereals": "open",
    "pasture": "open",
    "bush": "open",
    "glacier_firn": "glacier",
    "glacier_ice": "glacier",
    "rock": "rock",
    "fruits": "open",
    "vegetables": "open",
    "wheat": "open",
    "alpine_vegetation": "open",
    "wetland": "wetland",
    "rough_pasture": "open",
    "subalpine_meadow": "open",
    "alpine_meadow": "open",
    "bare_soil_vegetation": "open",
    "corn": "open",
    "grapes": "open",
}

#: Maximal interception storage [mm] of each land use, per month (January
#: first).
PREVAH_LAND_USE_SI_MAX: dict[str, list[float]] = {
    "water": [0] * 12,
    "urban": [5] * 12,
    "coniferous_forest": [2.5, 2.8, 2.8, 2.5, 3.5, 4.3, 4.5, 4.3, 4.1, 3.9, 3, 2.5],
    "deciduous_forest": [1.5, 1.5, 1.5, 2, 3, 3.5, 3.5, 3.3, 3.2, 2.7, 2, 1.5],
    "mixed_forest": [2, 2.2, 2.2, 2.3, 3.3, 3.9, 4, 3.8, 3.6, 3.2, 2.5, 2],
    "cereals": [0.5, 0.5, 0.5, 1.5, 2.5, 3, 3, 2.5, 2, 1.5, 1, 0.5],
    "pasture": [1.5, 1.5, 1.8, 2.3, 3, 3.5, 3.5, 3.2, 2.8, 2, 1.5, 1.5],
    "bush": [1.5, 1.5, 1.5, 2, 2.8, 3, 3, 2.8, 2.4, 2, 1.7, 1.5],
    "glacier_firn": [0.5] * 12,
    "glacier_ice": [0.5] * 12,
    "rock": [4] * 12,
    "fruits": [0.5, 0.5, 0.5, 1.5, 2.5, 3, 3, 2.5, 2, 1.5, 1, 0.5],
    "vegetables": [1.5, 1.5, 1.6, 2.1, 3, 3.5, 3.5, 3.2, 3, 2.4, 1.7, 1.5],
    "wheat": [0.5, 0.5, 0.5, 1, 1.5, 2, 2, 1.5, 0, 0.2, 0.3, 0.5],
    "alpine_vegetation": [0.5, 0.5, 0.5, 0.8, 0.8, 1.4, 1.5, 1.5, 1.4, 0.5, 0.5, 0.5],
    "wetland": [1, 1, 1.3, 1.8, 2.5, 2.8, 3, 2.8, 2, 1.3, 1, 1],
    "rough_pasture": [1, 1, 1.3, 1.8, 2.5, 2.8, 3, 2.8, 2, 1.3, 1, 1],
    "subalpine_meadow": [0.5, 0.5, 0.5, 0.8, 0.8, 1.2, 1.2, 1.2, 1.2, 0.5, 0.5, 0.5],
    "alpine_meadow": [0.5, 0.5, 0.5, 0.6, 0.8, 1.2, 1.2, 1.2, 1.2, 0.5, 0.5, 0.5],
    "bare_soil_vegetation": [0.1, 0.1, 0.1, 0.1, 0.2, 0.4,
                             0.4, 0.4, 0.3, 0.1, 0.1, 0.1],
    "corn": [0.1, 0.1, 0.1, 0.5, 1.5, 2, 2.2, 2.5, 2.5, 2, 0.1, 0.1],
    "grapes": [0.5, 0.5, 0.6, 1, 2, 2.5, 2.5, 2.2, 2, 0.9, 0.7, 0.5],
}

#: Fraction of the surface covered by vegetation [-], per land use and month.
PREVAH_LAND_USE_VEG_COV: dict[str, list[float]] = {
    "water": [0] * 12,
    "urban": [0.2] * 12,
    "coniferous_forest": [0.92, 0.92, 0.93, 0.94, 0.95, 0.97,
                          1, 1, 1, 0.98, 0.97, 0.96],
    "deciduous_forest": [0.6, 0.6, 0.6, 0.7, 0.85, 0.98, 1, 1, 0.95, 0.9, 0.75, 0.6],
    "mixed_forest": [0.76, 0.76, 0.78, 0.8, 0.87, 0.98, 1, 1, 0.97, 0.94, 0.82, 0.8],
    "cereals": [0.4, 0.4, 0.5, 0.6, 0.8, 1, 1, 1, 0.5, 0.3, 0.3, 0.4],
    "pasture": [0.9, 0.9, 0.92, 0.94, 0.96, 0.98, 0.98, 0.98, 0.95, 0.92, 0.9, 0.9],
    "bush": [0.9, 0.9, 0.92, 0.95, 0.99, 0.99, 0.99, 0.99, 0.96, 0.93, 0.9, 0.9],
    "glacier_firn": [0.1] * 12,
    "glacier_ice": [0] * 12,
    "rock": [0.1] * 12,
    "fruits": [0.4, 0.4, 0.5, 0.6, 0.8, 1, 1, 1, 0.5, 0.3, 0.3, 0.4],
    "vegetables": [0.7, 0.7, 0.75, 0.8, 0.9, 0.95, 0.95, 0.95, 0.95, 0.9, 0.8, 0.75],
    "wheat": [0.4, 0.4, 0.5, 0.6, 0.8, 1, 1, 1, 0, 0.2, 0.3, 0.4],
    "alpine_vegetation": [0.8, 0.8, 0.8, 0.8, 0.8, 0.9, 0.9, 0.9, 0.85, 0.8, 0.8, 0.8],
    "wetland": [0.8, 0.8, 0.8, 0.8, 0.8, 0.9, 0.9, 0.9, 0.85, 0.8, 0.8, 0.8],
    "rough_pasture": [0.7, 0.7, 0.7, 0.7, 0.7, 0.8, 0.8, 0.8, 0.75, 0.7, 0.7, 0.7],
    "subalpine_meadow": [0.6, 0.6, 0.6, 0.6, 0.6, 0.7, 0.7, 0.7, 0.65, 0.6, 0.6, 0.6],
    "alpine_meadow": [0.5, 0.5, 0.5, 0.5, 0.5, 0.6, 0.6, 0.6, 0.55, 0.5, 0.5, 0.5],
    "bare_soil_vegetation": [0.3, 0.3, 0.3, 0.3, 0.3, 0.4,
                             0.4, 0.4, 0.35, 0.3, 0.3, 0.3],
    "corn": [0.05, 0.05, 0.05, 0.5, 0.7, 0.8, 0.8, 0.9, 0.9, 0.8, 0.05, 0.05],
    "grapes": [0.5, 0.5, 0.55, 0.7, 0.9, 0.9, 0.9, 0.9, 0.9, 0.8, 0.6, 0.5],
}

#: Rooting depth [m], per land use and month. Drives the soil moisture capacity
#: (see :meth:`~hydrobricks.models.PrevahUniBE.land_use_field_capacity`).
PREVAH_LAND_USE_ROOT_DEPTH: dict[str, list[float]] = {
    "water": [0] * 12,
    "urban": [0.1] * 12,
    "coniferous_forest": [1.5] * 12,
    "deciduous_forest": [1.5] * 12,
    "mixed_forest": [1.5] * 12,
    "cereals": [0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.8, 0.8, 0.8, 0.2, 0.2, 0.2],
    "pasture": [0.6] * 12,
    "bush": [0.8] * 12,
    "glacier_firn": [0.2] * 12,
    "glacier_ice": [0.1] * 12,
    "rock": [0.1] * 12,
    "fruits": [0.2, 0.2, 0.2, 0.2, 0.5, 0.7, 0.8, 0.8, 0.5, 0.2, 0.2, 0.2],
    "vegetables": [0.8] * 12,
    "wheat": [0.2, 0.2, 0.2, 0.2, 0.4, 0.6, 0.8, 0.8, 0.8, 0.2, 0.2, 0.2],
    "alpine_vegetation": [0.5] * 12,
    "wetland": [0.3] * 12,
    "rough_pasture": [0.3] * 12,
    "subalpine_meadow": [0.3] * 12,
    "alpine_meadow": [0.3] * 12,
    "bare_soil_vegetation": [0.15] * 12,
    "corn": [0.6] * 12,
    "grapes": [0.8] * 12,
}

#: Soil moisture capacity [mm] PREVAH forces on the land cover types carrying no real
#: soil, whatever the soil map says (see
#: :meth:`~hydrobricks.models.PrevahUniBE.land_use_field_capacity`).
PREVAH_LAND_USE_FIELD_CAPACITY_FIXED: dict[str, float] = {
    "urban": 5.0,
    "rock": 3.0,
    "glacier": 0.1,
}

#: Minimum soil moisture capacity [mm] per land cover type, applied to the capacity
#: from the soil map. Open water holds a deep store; the vegetated covers use the
#: default below.
PREVAH_LAND_USE_FIELD_CAPACITY_MIN: dict[str, float] = {
    "water": 2500.0,
}

#: Minimum soil moisture capacity [mm] of the vegetated land covers.
PREVAH_LAND_USE_FIELD_CAPACITY_MIN_DEF: float = 10.0

#: Surface albedo [-], per land use and month. PREVAH derives it from the leaf
#: area index: ``albedo_bare(month) + 0.25 (albedo_veg - albedo_bare(month)) LAI``
#: when ``0 < LAI <= 4``, and the land use's own value otherwise.
PREVAH_LAND_USE_ALBEDO: dict[str, list[float]] = {
    "water": [0.05] * 12,
    "urban": [0.1, 0.1, 0.1, 0.1, 0.175, 0.175, 0.175, 0.175, 0.1, 0.1, 0.1, 0.1],
    "coniferous_forest": [0.12] * 12,
    "deciduous_forest": [0.10875, 0.10875, 0.10875, 0.17, 0.17, 0.17,
                         0.17, 0.17, 0.17, 0.17, 0.10875, 0.10875],
    "mixed_forest": [0.125, 0.125, 0.125, 0.14375, 0.15, 0.15,
                     0.15, 0.15, 0.15, 0.15, 0.125, 0.125],
    "cereals": [0.11875, 0.11875, 0.11875, 0.15625, 0.225, 0.25,
                0.25, 0.21875, 0.11875, 0.11875, 0.11875, 0.11875],
    "pasture": [0.11875, 0.11875, 0.1375, 0.2125, 0.25, 0.25,
                0.25, 0.25, 0.2125, 0.175, 0.11875, 0.11875],
    "bush": [0.10375, 0.10375, 0.10375, 0.1375, 0.25, 0.25,
             0.25, 0.25, 0.2125, 0.1375, 0.10375, 0.10375],
    "glacier_firn": [0.1, 0.1, 0.1, 0.1, 0.175, 0.1625,
                     0.15, 0.1625, 0.1, 0.1, 0.1, 0.1],
    "glacier_ice": [0.4] * 12,
    "rock": [0.12] * 12,
    "fruits": [0.11875, 0.11875, 0.11875, 0.15625, 0.225, 0.25,
               0.25, 0.21875, 0.11875, 0.11875, 0.11875, 0.11875],
    "vegetables": [0.105, 0.105, 0.105, 0.125, 0.2, 0.2,
                   0.2, 0.2, 0.2, 0.1625, 0.10625, 0.10625],
    "wheat": [0.11875, 0.11875, 0.11875, 0.15625, 0.225, 0.25,
              0.25, 0.21875, 0.11875, 0.11875, 0.11875, 0.11875],
    "alpine_vegetation": [0.1025, 0.1025, 0.1025, 0.1025, 0.2, 0.2,
                          0.2, 0.2, 0.175, 0.125, 0.1025, 0.1025],
    "wetland": [0.1025, 0.1025, 0.1025, 0.1025, 0.2, 0.2,
                0.2, 0.2, 0.175, 0.125, 0.1025, 0.1025],
    "rough_pasture": [0.10325, 0.10325, 0.10325, 0.10325, 0.21125, 0.2225,
                      0.23, 0.22625, 0.18125, 0.1325, 0.10325, 0.10325],
    "subalpine_meadow": [0.10325, 0.10325, 0.10325, 0.10325, 0.21125, 0.215,
                         0.21875, 0.2225, 0.1975, 0.1325, 0.10325, 0.10325],
    "alpine_meadow": [0.10375, 0.10375, 0.10375, 0.10375, 0.20625, 0.21875,
                      0.225, 0.225, 0.175, 0.1375, 0.10375, 0.10375],
    "bare_soil_vegetation": [0.1025, 0.1025, 0.1025, 0.1025, 0.2, 0.2,
                             0.2, 0.2, 0.1375, 0.1125, 0.1025, 0.1025],
    "corn": [0.1025, 0.1025, 0.1025, 0.1025, 0.2, 0.2,
             0.2, 0.2, 0.2, 0.2, 0.1025, 0.1025],
    "grapes": [0.105, 0.105, 0.105, 0.125, 0.2, 0.2,
               0.2, 0.2, 0.2, 0.1625, 0.10625, 0.10625],
}

#: Leaf area index [-], per land use and month. Provided for reference: it feeds
#: the albedo above and the Penman-Monteith resistances, which hydrobricks does
#: not implement (the PET is computed in preprocessing).
PREVAH_LAND_USE_LAI: dict[str, list[float]] = {
    "water": [0] * 12,
    "urban": [1] * 12,
    "coniferous_forest": [6, 6, 6, 6, 7, 8, 8, 8, 8, 7, 6, 6],
    "deciduous_forest": [0.5, 0.5, 0.5, 5, 6, 7, 7, 7, 6, 5, 0.5, 0.5],
    "mixed_forest": [2, 2, 2, 3.5, 6, 6, 6, 6, 6, 5.5, 2, 2],
    "cereals": [0.5, 0.5, 0.5, 1.5, 2, 4, 5, 1.5, 0.5, 0.5, 0.5, 0.5],
    "pasture": [0.5, 0.5, 1, 3, 5, 5, 5, 4, 3, 2, 0.5, 0.5],
    "bush": [0.1, 0.1, 0.1, 1, 4, 5, 5, 5, 3, 1, 0.1, 0.1],
    "glacier_firn": [0.2, 0.3, 0.4, 0.5, 1, 1.5, 2, 1.5, 1, 0.5, 0.3, 0.2],
    "glacier_ice": [0] * 12,
    "rock": [0] * 12,
    "fruits": [0.5, 0.5, 0.5, 1.5, 2, 4, 5, 1.5, 0.5, 0.5, 0.5, 0.5],
    "vegetables": [0.2, 0.2, 0.2, 1, 5, 5, 5, 5, 4, 2.5, 0.25, 0.25],
    "wheat": [0.5, 0.5, 0.5, 1.5, 2, 4, 5, 1.5, 0.5, 0.5, 0.5, 0.5],
    "alpine_vegetation": [0.1, 0.1, 0.1, 0.1, 1.5, 3, 4, 4, 3, 1, 0.1, 0.1],
    "wetland": [0.1, 0.1, 0.1, 0.1, 1.5, 3, 4, 4, 3, 1, 0.1, 0.1],
    "rough_pasture": [0.1, 0.1, 0.1, 0.1, 1.5, 3, 4, 3.5, 2.5, 1, 0.1, 0.1],
    "subalpine_meadow": [0.1, 0.1, 0.1, 0.1, 1.5, 2, 2.5, 3, 3, 1, 0.1, 0.1],
    "alpine_meadow": [0.1, 0.1, 0.1, 0.1, 0.5, 1.5, 2, 2, 2, 1, 0.1, 0.1],
    "bare_soil_vegetation": [0.1, 0.1, 0.1, 0.1, 0.5, 1, 1.5, 1.5, 1.5, 0.5, 0.1, 0.1],
    "corn": [0.1, 0.1, 0.1, 0.1, 0.1, 1, 3, 5, 5, 5, 0.1, 0.1],
    "grapes": [0.2, 0.2, 0.2, 1, 5, 5, 5, 5, 4, 2.5, 0.25, 0.25],
}

# fmt: on
