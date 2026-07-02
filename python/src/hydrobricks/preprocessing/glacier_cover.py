from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

import numpy as np
import pandas as pd

from hydrobricks._exceptions import ConfigurationError, DataError, DependencyError
from hydrobricks._optional import HAS_PYPROJ, HAS_SHAPELY, gpd

if TYPE_CHECKING:
    from hydrobricks.catchment import Catchment

if HAS_SHAPELY:
    from shapely.geometry import MultiPolygon


def extract_ice_thickness_and_mask(
    catchment: Catchment, ice_thickness: str | Path
) -> tuple[np.ndarray, np.ndarray]:
    """Extract the ice thickness raster and the corresponding glacier mask.

    Resamples the ice-thickness raster to the DEM resolution, zeroes it outside the
    catchment DEM, and derives a binary glacier presence mask (1 where the ice is
    thicker than 0).

    Parameters
    ----------
    catchment
        The catchment object (with its DEM extracted).
    ice_thickness
        Path to the TIF file containing the glacier thickness in meters.

    Returns
    -------
    A tuple ``(ice_thickness_data, glaciers_mask)`` where ``ice_thickness_data`` is
    the resampled thickness array (m) and ``glaciers_mask`` is a binary mask.
    """
    if not HAS_PYPROJ:
        raise DependencyError(
            "pyproj is required to process glacier ice thickness data.",
            package_name="pyproj",
            operation="glacier_cover.extract_ice_thickness_and_mask",
            install_command="pip install pyproj",
        )

    # Extract the ice thickness and resample it to the DEM resolution
    catchment.extract_attribute_raster(
        ice_thickness,
        "ice_thickness",
        resample_to_dem_resolution=True,
        resampling="average",
    )
    ice_thickness_data = catchment.attributes["ice_thickness"]["data"]
    ice_thickness_data[catchment.dem_data == 0] = 0.0

    glaciers_mask = np.zeros(catchment.dem_data.shape)
    glaciers_mask[ice_thickness_data > 0] = 1

    return ice_thickness_data, glaciers_mask


def extract_glacier_mask_from_shapefile(
    catchment: Catchment, glacier_outline: str | Path
) -> np.ndarray:
    """Extract the glacier cover mask from a glacier outline shapefile.

    Clips the glacier outlines to the catchment extent and rasterizes them onto the
    catchment DEM grid.

    Parameters
    ----------
    catchment
        The catchment object (with its DEM extracted).
    glacier_outline
        Path to the SHP file containing the glacier extents.

    Returns
    -------
    A mask array over the DEM grid, holding the DEM elevation where a glacier is
    present and ``-9999`` elsewhere.
    """
    # Clip the glaciers to the catchment extent
    all_glaciers = gpd.read_file(glacier_outline)
    all_glaciers.to_crs(catchment.crs, inplace=True)
    if catchment.outline[0].geom_type == "MultiPolygon":
        glaciers = gpd.clip(all_glaciers, catchment.outline[0])
    elif catchment.outline[0].geom_type == "Polygon":
        glaciers = gpd.clip(all_glaciers, MultiPolygon(catchment.outline))
    else:
        raise DataError(
            "The catchment outline must be a (multi)polygon.",
            data_type="catchment outline",
            reason="Invalid geometry type",
        )
    glaciers = _simplify_df_geometries(glaciers)

    # Get the glacier mask
    glaciers_mask = catchment.mask_dem(glaciers, -9999)

    return glaciers_mask


def initialize_glacier_cover_from_extent(
    catchment: Catchment,
    ice_thickness: str | Path | None = None,
    glacier_outline: str | Path | None = None,
    land_cover: str | None = None,
) -> None:
    """Initialize the glacier land cover of a catchment from a glacier extent.

    A lightweight alternative to
    ``GlacierEvolutionDeltaH.compute_initial_ice_thickness`` for when only the
    initial glacier cover is needed (not a delta-h / area-scaling lookup table). It
    extracts the glacier extent from an ice-thickness raster or a glacier-outline
    shapefile, computes the glacier area of each hydro unit, and sets the glacier
    land-cover fractions on the catchment. Unlike ``compute_initial_ice_thickness``
    it does not discretize the glacier into elevation bands nor compute per-band ice
    thicknesses, which are only needed for the glacier-evolution lookup tables.

    Parameters
    ----------
    catchment
        The (already discretized) catchment object.
    ice_thickness
        Path to the TIF file containing the glacier thickness in meters. Either this
        or ``glacier_outline`` must be provided (not both).
    glacier_outline
        Path to the SHP file containing the glacier extents. Either this or
        ``ice_thickness`` must be provided (not both).
    land_cover
        Name of the glacier land cover to initialize. If None (default), the single
        land cover of type 'glacier' is detected from the catchment.
    """
    if glacier_outline is None and ice_thickness is None:
        raise DataError(
            "Either glacier_outline or ice_thickness should be provided.",
            data_type="glacier",
            reason="Missing required parameter",
        )
    if glacier_outline is not None and ice_thickness is not None:
        raise DataError(
            "Either glacier_outline or ice_thickness should be provided, not both.",
            data_type="glacier",
            reason="Conflicting parameters",
        )
    if catchment.map_unit_ids is None:
        raise ConfigurationError(
            "Catchment has not been discretized. "
            "Please run create_elevation_bands() first.",
            reason="Catchment not initialized",
        )

    if ice_thickness is not None:
        _, glaciers_mask = extract_ice_thickness_and_mask(catchment, ice_thickness)
    else:
        glaciers_mask = extract_glacier_mask_from_shapefile(catchment, glacier_outline)

    glacier_df = _glacier_area_per_unit(catchment, glaciers_mask)
    initialize_glacier_cover(catchment, glacier_df, land_cover)


def initialize_glacier_cover(
    catchment: Catchment, glacier_df: pd.DataFrame, land_cover: str | None = None
) -> None:
    """Initialize the glacier cover of each hydro unit from per-unit glacier areas.

    Aggregates ``glacier_df`` (with ``(hydro_unit_id, -)`` and ``(glacier_area, m2)``
    columns) per hydro unit and sets the ``land_cover`` fractions on the catchment,
    so the glacier land cover starts with its actual area. When ``land_cover`` is
    None, the (single) land cover of type ``glacier`` is detected from the catchment.

    Parameters
    ----------
    catchment
        The (already discretized) catchment object.
    glacier_df
        DataFrame with ``(hydro_unit_id, -)`` and ``(glacier_area, m2)`` columns
        giving the glacier area of each hydro unit.
    land_cover
        Name of the glacier land cover to initialize. If None (default), the single
        land cover of type 'glacier' is detected from the catchment.
    """
    if land_cover is None:
        land_cover = detect_glacier_cover(catchment)
    areas = (
        pd.DataFrame(
            {
                "hydro_unit": glacier_df[("hydro_unit_id", "-")].to_numpy(),
                "area": glacier_df[("glacier_area", "m2")].to_numpy(),
            }
        )
        .groupby("hydro_unit", as_index=False)["area"]
        .sum()
    )
    catchment.initialize_area_from_land_cover_change(land_cover, areas)


def detect_glacier_cover(catchment: Catchment) -> str:
    """Return the name of the (single) land cover of type ``glacier``.

    Parameters
    ----------
    catchment
        The catchment object.

    Returns
    -------
    The name of the land cover of type 'glacier'.

    Raises
    ------
    ConfigurationError
        If there is no glacier cover, or more than one (in which case the caller
        must pass ``land_cover`` explicitly).
    """
    glacier_covers = [
        name
        for name, cover_type in zip(
            catchment.hydro_units.land_cover_names,
            catchment.hydro_units.land_cover_types,
        )
        if cover_type == "glacier"
    ]
    if len(glacier_covers) != 1:
        raise ConfigurationError(
            "Could not determine the glacier land cover automatically "
            f"(found {glacier_covers}); pass land_cover explicitly.",
            item_name="land_cover",
            item_value=glacier_covers,
            reason="Expected exactly one land cover of type 'glacier'",
        )
    return glacier_covers[0]


def _glacier_area_per_unit(
    catchment: Catchment, glaciers_mask: np.ndarray
) -> pd.DataFrame:
    """Compute the glacier area of each hydro unit from a glacier mask.

    Returns a DataFrame with ``(hydro_unit_id, -)`` and ``(glacier_area, m2)``
    columns, as expected by ``initialize_glacier_cover``.
    """
    px_area = catchment.get_dem_pixel_area()

    unit_ids = np.unique(catchment.map_unit_ids[glaciers_mask > 0])
    unit_ids = unit_ids[unit_ids != 0]

    rows = []
    for unit_id in unit_ids:
        mask_unit = catchment.map_unit_ids[glaciers_mask > 0] == unit_id
        area = np.count_nonzero(mask_unit) * px_area
        rows.append([unit_id, area])

    return pd.DataFrame(rows, columns=[("hydro_unit_id", "-"), ("glacier_area", "m2")])


def _simplify_df_geometries(df: gpd.GeoDataFrame) -> gpd.GeoDataFrame:
    # Merge the polygons
    df["new_col"] = 0
    df = df.dissolve(by="new_col", as_index=False)
    # Drop all columns except the geometry
    df = df[["geometry"]]

    return df
