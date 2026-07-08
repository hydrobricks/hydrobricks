from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING, ClassVar

import numpy as np
import pandas as pd

from hydrobricks._exceptions import ConfigurationError, DataError, DependencyError
from hydrobricks._optional import (
    HAS_GEOPANDAS,
    HAS_RASTERIO,
    HAS_SHAPELY,
    gpd,
)

if TYPE_CHECKING:
    from hydrobricks.catchment import Catchment


class CatchmentLandCover:
    """
    Extract per-hydro-unit land-cover fractions from raster or vector datasets.

    Maps the classes of a land-cover dataset (e.g. ESA WorldCover, CORINE,
    swissTLMRegio) to hydrobricks land covers and sets the fractions of each hydro
    unit. Accessible via ``catchment.land_cover``; the ``Catchment`` also exposes the
    ``extract_land_cover_from_raster`` / ``extract_land_cover_from_shapefile``
    passthroughs.

    Built-in class-to-cover mappings are available as class constants and selected via
    the ``dataset`` argument (see :attr:`PRESETS`).
    """

    #: ESA WorldCover 2021 v200 discrete classification (10 m).
    #: Class codes: https://esa-worldcover.org/
    ESA_WORLDCOVER_2021: ClassVar[dict[int, str]] = {
        10: "forest",  # Tree cover
        20: "open",  # Shrubland
        30: "open",  # Grassland
        40: "open",  # Cropland
        50: "open",  # Built-up (no dedicated urban brick yet)
        60: "open",  # Bare / sparse vegetation
        70: "glacier",  # Snow and ice
        80: "lake",  # Permanent water bodies
        90: "wetland",  # Herbaceous wetland
        95: "wetland",  # Mangroves
        100: "open",  # Moss and lichen
    }

    #: CORINE Land Cover, keyed by the standard 3-digit CLC level-3 codes (``CODE_18``).
    #: Note: CORINE raster products are often distributed with a 1..44 grid index rather
    #: than these semantic codes; for such rasters, supply a custom ``mapping`` (or use
    #: the vector CLC product, whose ``CODE_18`` attribute matches these keys).
    CORINE: ClassVar[dict[int, str]] = {
        # Artificial surfaces -> generic soil (no dedicated urban brick yet)
        111: "open",  # Continuous urban fabric
        112: "open",  # Discontinuous urban fabric
        121: "open",  # Industrial or commercial units
        122: "open",  # Road and rail networks and associated land
        123: "open",  # Port areas
        124: "open",  # Airports
        131: "open",  # Mineral extraction sites
        132: "open",  # Dump sites
        133: "open",  # Construction sites
        141: "open",  # Green urban areas
        142: "open",  # Sport and leisure facilities
        # Agricultural areas -> generic soil
        211: "open",  # Non-irrigated arable land
        212: "open",  # Permanently irrigated land
        213: "open",  # Rice fields
        221: "open",  # Vineyards
        222: "open",  # Fruit trees and berry plantations
        223: "open",  # Olive groves
        231: "open",  # Pastures
        241: "open",  # Annual crops associated with permanent crops
        242: "open",  # Complex cultivation patterns
        243: "open",  # Principally agriculture; significant areas of natural vegetation
        244: "open",  # Agro-forestry areas
        # Forests
        311: "forest",  # Broad-leaved forest
        312: "forest",  # Coniferous forest
        313: "forest",  # Mixed forest
        # Scrub / herbaceous vegetation
        321: "open",  # Natural grasslands
        322: "open",  # Moors and heathland
        323: "open",  # Sclerophyllous vegetation
        324: "forest",  # Transitional woodland-shrub
        # Open spaces with little or no vegetation
        331: "open",  # Beaches, dunes, sands
        332: "open",  # Bare rocks
        333: "open",  # Sparsely vegetated areas
        334: "open",  # Burnt areas
        335: "glacier",  # Glaciers and perpetual snow
        # Wetlands / others
        411: "wetland",  # Inland marshes
        412: "wetland",  # Peatbogs
        421: "wetland",  # Salt marshes
        422: "lake",  # Salines
        423: "lake",  # Intertidal flats
        # Water bodies -> lake
        511: "lake",  # Water courses
        512: "lake",  # Water bodies
        521: "lake",  # Coastal lagoons
        522: "lake",  # Estuaries
        523: "lake",  # Sea and ocean
    }

    #: Registry of named presets accepted by the ``dataset`` argument.
    PRESETS: ClassVar[dict[str, dict[int, str]]] = {
        "esa_worldcover": ESA_WORLDCOVER_2021,
        "corine": CORINE,
    }

    #: Cover name -> hydrobricks land-cover type, used when auto-registering a cover
    #: that the mapping targets but the catchment does not yet declare. Names not listed
    #: here default to the generic land cover brick.
    KNOWN_COVER_TYPES: ClassVar[dict[str, str]] = {
        "glacier": "glacier",
        "forest": "forest",
        "lake": "lake",
        "open": "open",
        "ground": "generic_land_cover",
        "generic": "generic_land_cover",
        "generic_land_cover": "generic_land_cover",
        "urban": "generic_land_cover",  # no dedicated urban brick yet
    }

    def __init__(self, catchment: Catchment) -> None:
        """
        Initialize the land-cover extraction module.

        Parameters
        ----------
        catchment
            The catchment object.
        """
        self.catchment: Catchment = catchment

    def extract_from_raster(
        self,
        raster_path: str | Path,
        mapping: dict | None = None,
        dataset: str | None = None,
    ) -> None:
        """Extract land-cover fractions from a categorical raster (e.g. ESA WorldCover).

        Ingests a categorical land-cover raster onto the DEM grid (reprojecting it if
        its CRS differs from the catchment, e.g. WorldCover in EPSG:4326 against a
        projected DEM), maps each source class to a hydrobricks land cover, and sets the
        per-hydro-unit fractions. Covers targeted by the mapping that the catchment does
        not yet declare are registered automatically. Classes without a mapping entry
        (and cells outside the raster coverage) fall into the residual absorbed by the
        generic (soil) cover.

        Must be called after the catchment has been discretized and before
        ``Model.setup()``.

        Parameters
        ----------
        raster_path
            Path to the categorical land-cover raster (GeoTIFF).
        mapping
            Dict mapping source class codes to hydrobricks land-cover names, e.g.
            ``{10: 'forest', 70: 'glacier'}``. Overrides/extends the ``dataset`` preset
            when both are given. Required if ``dataset`` is None.
        dataset
            Name of a built-in preset mapping: ``'esa_worldcover'`` or ``'corine'``.
        """
        catchment = self.catchment
        self._check_dependencies(vector=False)
        self._check_discretized()
        mapping = self._resolve_mapping(mapping, dataset)

        # Ingest the categorical raster onto the DEM grid (CRS-aware, nearest neighbour
        # to preserve class values).
        catchment.extract_attribute_raster(
            raster_path,
            "land_cover",
            resample_to_dem_resolution=True,
            resampling="nearest",
            replace_nans_by_zeros=True,
            reproject_crs=True,
        )
        class_data = catchment.attributes["land_cover"]["data"]
        classes = np.rint(class_data).astype(int)

        cover_areas = self._areas_from_class_grid(classes, mapping)
        self._apply_cover_areas(cover_areas)

    def extract_from_shapefile(
        self,
        shapefile_path: str | Path,
        class_field: str,
        mapping: dict | None = None,
        dataset: str | None = None,
        all_touched: bool = True,
    ) -> None:
        """Extract land-cover fractions from a vector dataset (e.g. swissTLMRegio).

        Reads a land-cover polygon layer, reprojects it to the catchment CRS, rasterizes
        each class onto the DEM grid (each cell is assigned to a single class, first
        match in the mapping wins), maps classes to hydrobricks land covers, and sets
        the per-hydro-unit fractions. Covers targeted by the mapping that the catchment
        does not yet declare are registered automatically. Cells not covered by any
        mapped class fall into the residual absorbed by the generic (soil) cover.

        Must be called after the catchment has been discretized and before
        ``Model.setup()``.

        Parameters
        ----------
        shapefile_path
            Path to the land-cover vector file (any format readable by geopandas).
        class_field
            Name of the attribute field holding the source class code of each feature.
        mapping
            Dict mapping source class codes (matching ``class_field`` values) to
            hydrobricks land-cover names. Overrides/extends the ``dataset`` preset when
            both are given. Required if ``dataset`` is None.
        dataset
            Name of a built-in preset mapping: ``'esa_worldcover'`` or ``'corine'``.
        all_touched
            If True (default), a DEM cell is assigned to a class as soon as its geometry
            touches the cell (includes partly-covered border cells). If False, only
            cells whose centre falls within a polygon are assigned.
        """
        self._check_dependencies(vector=True)
        self._check_discretized()
        mapping = self._resolve_mapping(mapping, dataset)

        gdf = gpd.read_file(shapefile_path)
        gdf = gdf.to_crs(self.catchment.crs)
        if class_field not in gdf.columns:
            raise DataError(
                f"The field '{class_field}' is not present in the shapefile.",
                data_type="land cover shapefile",
                reason="Missing class field",
            )

        classes = self._rasterize_classes(gdf, class_field, mapping, all_touched)
        cover_areas = self._areas_from_class_grid(classes, mapping)
        self._apply_cover_areas(cover_areas)

    def _rasterize_classes(
        self,
        gdf: gpd.GeoDataFrame,
        class_field: str,
        mapping: dict,
        all_touched: bool,
    ) -> np.ndarray:
        """Burn the mapped vector classes onto the DEM grid (1 assignment per cell).

        Returns an integer grid over the DEM holding, for each cell, the source class
        code of the first mapped class covering it (0 where no mapped class applies).
        """
        catchment = self.catchment
        classes = np.zeros(catchment.map_unit_ids.shape, dtype=int)
        assigned = np.zeros(catchment.map_unit_ids.shape, dtype=bool)

        for class_value in mapping:
            subset = gdf[gdf[class_field] == class_value]
            if subset.empty:
                continue
            masked = catchment.mask_dem(subset, nodata=-9999, all_touched=all_touched)
            present = (masked != -9999) & ~assigned
            classes[present] = self._as_int_class(class_value)
            assigned |= present

        return classes

    def _areas_from_class_grid(
        self, classes: np.ndarray, mapping: dict
    ) -> dict[str, dict[int, float]]:
        """Aggregate per-hydro-unit areas of each target cover from a class grid.

        Returns a nested dict ``{cover_name: {hydro_unit_id: area_m2}}``.
        """
        catchment = self.catchment
        px_area = catchment.get_dem_pixel_area()
        map_ids = catchment.map_unit_ids

        cover_areas: dict[str, dict[int, float]] = {}
        for class_value, cover_name in mapping.items():
            class_mask = classes == self._as_int_class(class_value)
            masked_ids = map_ids[class_mask]
            unit_ids = np.unique(masked_ids)
            unit_ids = unit_ids[unit_ids != 0]
            for unit_id in unit_ids:
                area = np.count_nonzero(masked_ids == unit_id) * px_area
                per_unit = cover_areas.setdefault(cover_name, {})
                per_unit[int(unit_id)] = per_unit.get(int(unit_id), 0.0) + area

        return cover_areas

    def _apply_cover_areas(self, cover_areas: dict[str, dict[int, float]]) -> None:
        """Register missing covers and set their fractions from per-unit areas.

        Generic (soil) covers are skipped: they absorb the residual area automatically.
        """
        catchment = self.catchment
        hu = catchment.hydro_units
        generic_aliases = hu.GENERIC_COVER_ALIASES

        for cover_name in sorted(cover_areas):
            if cover_name not in hu.land_cover_names:
                hu.add_land_cover(cover_name, self._resolve_cover_type(cover_name))

            cover_type = hu.land_cover_types[hu.land_cover_names.index(cover_name)]
            if cover_name in generic_aliases or cover_type in generic_aliases:
                continue

            unit_areas = cover_areas[cover_name]
            areas = pd.DataFrame(
                {
                    "hydro_unit": list(unit_areas.keys()),
                    "area": list(unit_areas.values()),
                }
            )
            catchment.initialize_area_from_land_cover_change(cover_name, areas)

    def _resolve_mapping(self, mapping: dict | None, dataset: str | None) -> dict:
        """Combine a preset mapping (by ``dataset`` name) with a user override."""
        resolved: dict = {}
        if dataset is not None:
            if dataset not in self.PRESETS:
                raise ConfigurationError(
                    f"Unknown land-cover dataset preset '{dataset}'.",
                    item_name="dataset",
                    item_value=dataset,
                    reason=f"Expected one of {sorted(self.PRESETS)}",
                )
            resolved.update(self.PRESETS[dataset])
        if mapping is not None:
            resolved.update(mapping)
        if not resolved:
            raise DataError(
                "No land-cover mapping provided. Pass a 'mapping' dict and/or a "
                "'dataset' preset name.",
                data_type="land cover mapping",
                reason="Missing mapping",
            )
        return resolved

    def _resolve_cover_type(self, cover_name: str) -> str:
        """Resolve the hydrobricks land-cover type for a target cover name."""
        return self.KNOWN_COVER_TYPES.get(cover_name, "generic_land_cover")

    @staticmethod
    def _as_int_class(class_value) -> int:
        """Coerce a class code to int for grid comparison (raster codes are numeric)."""
        return int(class_value)

    def _check_discretized(self) -> None:
        if self.catchment.map_unit_ids is None:
            raise ConfigurationError(
                "Catchment has not been discretized. "
                "Please run create_elevation_bands() or discretize_by() first.",
                reason="Catchment not discretized",
            )

    @staticmethod
    def _check_dependencies(vector: bool) -> None:
        if not HAS_RASTERIO:
            raise DependencyError(
                "rasterio is required to extract land cover.",
                package_name="rasterio",
                operation="CatchmentLandCover",
                install_command="pip install rasterio",
            )
        if vector and not HAS_GEOPANDAS:
            raise DependencyError(
                "geopandas is required to extract land cover from a shapefile.",
                package_name="geopandas",
                operation="CatchmentLandCover",
                install_command="pip install geopandas",
            )
        if vector and not HAS_SHAPELY:
            raise DependencyError(
                "shapely is required to extract land cover from a shapefile.",
                package_name="shapely",
                operation="CatchmentLandCover",
                install_command="pip install shapely",
            )
