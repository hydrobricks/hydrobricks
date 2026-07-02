import os.path
import tempfile
from pathlib import Path

import numpy as np
import pytest

import hydrobricks as hb
from hydrobricks._exceptions import ConfigurationError, DataError

FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
)
SITTER_OUTLINE = FILES_DIR / "ch_sitter_appenzell" / "outline.shp"
SITTER_DEM = FILES_DIR / "ch_sitter_appenzell" / "dem.tif"


def has_required_packages() -> bool:
    return hb.HAS_RASTERIO and hb.HAS_GEOPANDAS and hb.HAS_SHAPELY


def _discretized_catchment(land_cover_types=None, land_cover_names=None):
    catchment = hb.Catchment(
        SITTER_OUTLINE,
        land_cover_types=land_cover_types,
        land_cover_names=land_cover_names,
    )
    catchment.extract_dem(SITTER_DEM)
    catchment.create_elevation_bands(method="equal_intervals", distance=50)
    return catchment


def _write_worldcover_raster(path, catchment, class_grid_fn):
    """Write a small categorical land-cover raster in EPSG:4326 over the catchment.

    ``class_grid_fn(rows, cols)`` returns the uint8 class array to burn.
    """
    from rasterio.transform import from_bounds
    from rasterio.warp import transform_bounds

    b = catchment.dem.bounds
    # Transform the DEM bounds to EPSG:4326 and pad a little to fully cover the DEM.
    left, bottom, right, top = transform_bounds(catchment.crs, "EPSG:4326", *b)
    pad = 0.002
    left, bottom, right, top = left - pad, bottom - pad, right + pad, top + pad

    rows, cols = 60, 60
    transform = from_bounds(left, bottom, right, top, cols, rows)
    data = class_grid_fn(rows, cols)

    with hb.rasterio.open(
        path,
        "w",
        driver="GTiff",
        height=rows,
        width=cols,
        count=1,
        dtype="uint8",
        crs="EPSG:4326",
        transform=transform,
        nodata=0,
    ) as dst:
        dst.write(data, 1)


def _write_polygon_shapefile(path, catchment, classes):
    """Write a vector layer splitting the catchment bbox into vertical class bands.

    ``classes`` is a list of class codes; the bbox is split into equal vertical bands,
    one per class, in the catchment CRS.
    """
    from shapely.geometry import box

    b = catchment.dem.bounds
    n = len(classes)
    width = (b.right - b.left) / n
    geoms = []
    for i in range(n):
        x0 = b.left + i * width
        x1 = b.left + (i + 1) * width
        geoms.append(box(x0, b.bottom, x1, b.top))

    gdf = hb.gpd.GeoDataFrame({"lc": classes, "geometry": geoms}, crs=catchment.crs)
    gdf.to_file(path)


def test_extract_land_cover_from_raster_uniform():
    if not has_required_packages():
        return
    catchment = _discretized_catchment()

    with tempfile.TemporaryDirectory() as tmp_dir:
        raster = Path(tmp_dir) / "worldcover.tif"
        # Whole extent is tree cover (WorldCover class 10 -> forest).
        _write_worldcover_raster(
            raster, catchment, lambda r, c: np.full((r, c), 10, dtype="uint8")
        )
        catchment.extract_land_cover_from_raster(raster, dataset="esa_worldcover")

    hu = catchment.hydro_units
    assert "forest" in hu.land_cover_names  # auto-registered
    forest = hu.hydro_units[("fraction-forest", "fraction")].to_numpy()
    open_cover = hu.hydro_units[("fraction-open", "fraction")].to_numpy()

    assert np.allclose(forest, 1.0, atol=1e-6)
    assert np.allclose(open_cover, 0.0, atol=1e-6)


def test_extract_land_cover_from_raster_mixed_sums_to_one():
    if not has_required_packages():
        return
    catchment = _discretized_catchment()

    def split(rows, cols):
        grid = np.full((rows, cols), 10, dtype="uint8")  # forest
        grid[:, cols // 2 :] = 70  # snow & ice -> glacier
        return grid

    with tempfile.TemporaryDirectory() as tmp_dir:
        raster = Path(tmp_dir) / "worldcover.tif"
        _write_worldcover_raster(raster, catchment, split)
        catchment.extract_land_cover_from_raster(raster, dataset="esa_worldcover")

    hu = catchment.hydro_units
    assert "forest" in hu.land_cover_names
    assert "glacier" in hu.land_cover_names

    forest = hu.hydro_units[("fraction-forest", "fraction")].to_numpy()
    glacier = hu.hydro_units[("fraction-glacier", "fraction")].to_numpy()
    open_cover = hu.hydro_units[("fraction-open", "fraction")].to_numpy()
    total = forest + glacier + open_cover

    assert forest.max() > 0
    assert glacier.max() > 0
    assert np.all(forest >= -1e-9) and np.all(forest <= 1 + 1e-9)
    assert np.all(glacier >= -1e-9) and np.all(glacier <= 1 + 1e-9)
    assert np.allclose(total, 1.0, atol=1e-6)


def test_extract_land_cover_from_raster_user_mapping():
    if not has_required_packages():
        return
    catchment = _discretized_catchment()

    with tempfile.TemporaryDirectory() as tmp_dir:
        raster = Path(tmp_dir) / "cover.tif"
        _write_worldcover_raster(
            raster, catchment, lambda r, c: np.full((r, c), 5, dtype="uint8")
        )
        # Custom class code mapped to a user-defined cover name.
        catchment.extract_land_cover_from_raster(raster, mapping={5: "forest"})

    forest = catchment.hydro_units.hydro_units[
        ("fraction-forest", "fraction")
    ].to_numpy()
    assert np.allclose(forest, 1.0, atol=1e-6)


def test_extract_land_cover_from_raster_requires_mapping():
    if not has_required_packages():
        return
    catchment = _discretized_catchment()
    with tempfile.TemporaryDirectory() as tmp_dir:
        raster = Path(tmp_dir) / "cover.tif"
        _write_worldcover_raster(
            raster, catchment, lambda r, c: np.full((r, c), 10, dtype="uint8")
        )
        with pytest.raises(DataError):
            catchment.extract_land_cover_from_raster(raster)
        with pytest.raises(ConfigurationError):
            catchment.extract_land_cover_from_raster(raster, dataset="unknown")


def test_extract_land_cover_requires_discretization():
    if not has_required_packages():
        return
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    with tempfile.TemporaryDirectory() as tmp_dir:
        raster = Path(tmp_dir) / "cover.tif"
        # DEM is loaded so we can build the raster, but no discretization was run.
        _write_worldcover_raster(
            raster, catchment, lambda r, c: np.full((r, c), 10, dtype="uint8")
        )
        with pytest.raises(ConfigurationError):
            catchment.extract_land_cover_from_raster(raster, dataset="esa_worldcover")


def test_extract_land_cover_from_shapefile_uniform():
    if not has_required_packages():
        return
    catchment = _discretized_catchment()

    with tempfile.TemporaryDirectory() as tmp_dir:
        shp = Path(tmp_dir) / "landcover.shp"
        # A single band covering the whole catchment, class 10 -> forest.
        _write_polygon_shapefile(shp, catchment, [10])
        catchment.extract_land_cover_from_shapefile(
            shp, class_field="lc", dataset="esa_worldcover"
        )

    hu = catchment.hydro_units
    assert "forest" in hu.land_cover_names
    forest = hu.hydro_units[("fraction-forest", "fraction")].to_numpy()
    assert not np.isnan(forest).any()
    assert np.allclose(forest, 1.0, atol=1e-6)


def test_extract_land_cover_from_shapefile_two_classes():
    if not has_required_packages():
        return
    catchment = _discretized_catchment()

    with tempfile.TemporaryDirectory() as tmp_dir:
        shp = Path(tmp_dir) / "landcover.shp"
        _write_polygon_shapefile(shp, catchment, [10, 70])  # forest | glacier
        catchment.extract_land_cover_from_shapefile(
            shp, class_field="lc", dataset="esa_worldcover"
        )

    hu = catchment.hydro_units
    forest = hu.hydro_units[("fraction-forest", "fraction")].to_numpy()
    glacier = hu.hydro_units[("fraction-glacier", "fraction")].to_numpy()
    open_cover = hu.hydro_units[("fraction-open", "fraction")].to_numpy()
    total = forest + glacier + open_cover

    assert forest.max() > 0
    assert glacier.max() > 0
    assert np.allclose(total, 1.0, atol=1e-6)


def test_discretize_by_sub_catchments():
    if not has_required_packages():
        return

    # Reference: elevation-only discretization.
    reference = _discretized_catchment()
    n_elev = reference.hydro_units.get_hydro_unit_count()

    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)

    with tempfile.TemporaryDirectory() as tmp_dir:
        shp = Path(tmp_dir) / "sub_catchments.shp"
        # Two vertical sub-catchments tiling the catchment bbox.
        _write_polygon_shapefile(shp, catchment, [1, 2])
        catchment.discretize_by(
            ["elevation", "sub_catchments"],
            elevation_distance=50,
            sub_catchments=shp,
        )

        hu = catchment.hydro_units
        assert hu.has("sub_catchment")

        sub_values = set(hu.hydro_units[("sub_catchment", "-")].to_numpy().tolist())
        assert sub_values == {"1", "2"}

        # Splitting by sub-catchments produces more units than elevation alone.
        assert hu.get_hydro_unit_count() > n_elev

        # Key invariant: no hydro unit spans two sub-catchments.
        sub_map, _ = catchment.discretization._build_sub_catchments_map(shp)
        ids = np.unique(catchment.map_unit_ids)
        ids = ids[ids != 0]
        for unit_id in ids:
            covered = np.unique(sub_map[catchment.map_unit_ids == unit_id])
            covered = covered[covered != 0]
            assert len(covered) == 1


def test_discretize_by_sub_catchments_requires_file():
    if not has_required_packages():
        return
    catchment = hb.Catchment(SITTER_OUTLINE)
    catchment.extract_dem(SITTER_DEM)
    with pytest.raises(ConfigurationError):
        catchment.discretize_by(["elevation", "sub_catchments"], elevation_distance=50)
