"""Land cover type names shared across the models and the hydro units."""

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
