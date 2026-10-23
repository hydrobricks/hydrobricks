"""Delineation of the subbasins of a river network from outlet points.

The Sitter at Appenzell (CAMELS-CH 2112, 74 km2) is nested in the Sitter at
St. Gallen (2468, 261 km2): one outlet at the Appenzell gauge splits the St. Gallen
catchment in two subbasins.
"""

import os.path
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

import hydrobricks as hb
import hydrobricks.models as models

pytest.importorskip("pysheds")
pytest.importorskip("rasterio")
pytest.importorskip("geopandas")

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
)
STGALLEN_DIR = TEST_FILES_DIR / "ch_sitter_stgallen"
APPENZELL_DIR = TEST_FILES_DIR / "ch_sitter_appenzell"

# The Sitter at the Appenzell gauge, in the catchment CRS (EPSG:2056), a few metres
# off the DEM stream so that the snapping is exercised.
APPENZELL_GAUGE = (2749050.0, 1244200.0)
APPENZELL_AREA = 74.4e6  # m2, from the official outline


@pytest.fixture(scope="module")
def catchment():
    catchment = hb.Catchment(STGALLEN_DIR / "outline.shp")
    catchment.extract_dem(STGALLEN_DIR / "dem.tif")
    return catchment


def _area(catchment):
    """The catchment area [m2] on the DEM grid."""
    cells = np.count_nonzero(~np.isnan(catchment.dem_data))
    return cells * catchment.get_dem_pixel_area()


def test_delineation_requires_a_dem_and_one_source():
    bare = hb.Catchment(STGALLEN_DIR / "outline.shp")
    with pytest.raises(hb.ConfigurationError):
        bare.delineate_subbasins([APPENZELL_GAUGE])
    bare.extract_dem(STGALLEN_DIR / "dem.tif")
    # Either outlet points or polygons, not both nor none.
    with pytest.raises(hb.ConfigurationError):
        bare.delineate_subbasins()
    with pytest.raises(hb.ConfigurationError):
        bare.delineate_subbasins(
            [APPENZELL_GAUGE], subbasins=APPENZELL_DIR / "outline.shp"
        )


def test_hydro_units_before_the_subbasins_are_refused():
    catchment = hb.Catchment(STGALLEN_DIR / "outline.shp")
    catchment.extract_dem(STGALLEN_DIR / "dem.tif")
    catchment.load_unit_ids_from_raster(STGALLEN_DIR / "unit_ids.tif")
    with pytest.raises(hb.ConfigurationError, match="delineate the subbasins first"):
        catchment.delineate_subbasins([APPENZELL_GAUGE])


def test_delineate_a_nested_gauge(catchment, tmp_path):
    table = catchment.delineate_subbasins([APPENZELL_GAUGE], names=["Appenzell"])

    # Two subbasins: the Appenzell gauge and the remainder down to St. Gallen.
    assert list(table["id"]) == [1, 2]
    assert list(table["downstream"]) == [2, 0]
    assert list(table["name"]) == ["Appenzell", "outlet"]
    assert table.loc[0, "area_drained"] == pytest.approx(APPENZELL_AREA, rel=0.25)
    assert table.loc[1, "area_drained"] == pytest.approx(_area(catchment), rel=0.01)
    assert table.loc[1, "area_local"] + table.loc[0, "area_local"] == pytest.approx(
        table.loc[1, "area_drained"]
    )
    # The outlet reach runs from the Appenzell gauge down to St. Gallen.
    assert table.loc[1, "length"] > 5000
    assert 0 < table.loc[1, "slope"] < 0.1
    assert table.loc[0, "elevation_outlet"] > table.loc[1, "elevation_outlet"]
    # The headwater gets its longest flow path as reach length.
    assert table.loc[0, "length"] > 1000
    # The snapped outlet stays close to the gauge.
    assert abs(table.loc[0, "outlet_x"] - APPENZELL_GAUGE[0]) < 200
    assert abs(table.loc[0, "outlet_y"] - APPENZELL_GAUGE[1]) < 200

    # The delineated Appenzell subbasin covers the official outline.
    import geopandas as gpd
    from rasterio.features import geometry_mask

    outline = gpd.read_file(APPENZELL_DIR / "outline.shp").to_crs(catchment.crs)
    inside = ~geometry_mask(
        list(outline.geometry),
        transform=catchment.dem.transform,
        invert=False,
        out_shape=catchment.map_subbasin_ids.shape,
    )
    covered = np.count_nonzero(catchment.map_subbasin_ids[inside] == 1) / inside.sum()
    assert covered > 0.95

    # The raster round-trips.
    catchment.save_subbasin_ids_raster(tmp_path)
    import rasterio

    with rasterio.open(tmp_path / "subbasin_ids.tif") as src:
        values = src.read(1)
    assert set(np.unique(values)) == {0, 1, 2}
    assert (values == catchment.map_subbasin_ids).all()


def test_outlet_far_from_a_stream_is_refused(catchment):
    with pytest.raises(hb.DataError, match="No stream cell"):
        catchment.delineate_subbasins([(2736000.0, 1254000.0)], snap_distance=50.0)


def test_hydro_units_created_within_the_subbasins(tmp_path):
    catchment = hb.Catchment(STGALLEN_DIR / "outline.shp")
    catchment.extract_dem(STGALLEN_DIR / "dem.tif")
    table = catchment.delineate_subbasins({"Appenzell": APPENZELL_GAUGE})
    assert list(table["name"]) == ["Appenzell", "outlet"]
    catchment.create_elevation_bands(method="equal_intervals", distance=100)

    # Every unit lies in a single subbasin, and the units carry the table.
    hydro_units = catchment.hydro_units
    unit_ids = hydro_units.hydro_units["id"].iloc[:, 0].to_numpy()
    for unit_id in unit_ids:
        cells = catchment.map_subbasin_ids[catchment.map_unit_ids == unit_id]
        assert len(np.unique(cells)) == 1
    assert hydro_units.has("elevation") and hydro_units.has("slope")
    assert hydro_units.get_subbasin_ids() == [1, 2]
    areas = hydro_units.get_subbasin_areas()
    assert areas.loc[2, "drained"] == pytest.approx(_area(catchment), rel=0.01)

    # A unit raster drawn within the subbasins can be loaded after the delineation.
    catchment.save_unit_ids_raster(tmp_path)
    reloaded = hb.Catchment(STGALLEN_DIR / "outline.shp")
    reloaded.extract_dem(STGALLEN_DIR / "dem.tif")
    reloaded.delineate_subbasins({"Appenzell": APPENZELL_GAUGE})
    reloaded.load_unit_ids_from_raster(tmp_path / "unit_ids.tif")
    reloaded.get_hydro_units_attributes()
    column = reloaded.hydro_units.hydro_units["subbasin"].iloc[:, 0].to_numpy()
    expected = hydro_units.hydro_units["subbasin"].iloc[:, 0].to_numpy()
    assert list(column) == list(expected)
    assert reloaded.hydro_units.get_subbasin_ids() == [1, 2]

    # The units run as a two-subbasin model with channel routing.
    hydro_units.save_to_csv(tmp_path / "hydro_units.csv")
    table.to_csv(tmp_path / "subbasins.csv", index=False)
    units = hb.HydroUnits()
    units.load_from_csv(tmp_path / "hydro_units.csv")
    units.set_subbasins(tmp_path / "subbasins.csv")

    forcing = hb.Forcing(units)
    forcing.load_station_data_from_csv(
        STGALLEN_DIR / "meteo.csv",
        column_time="date",
        time_format="%d/%m/%Y",
        content={
            "precipitation": "precip(mm/day)",
            "temperature": "temp(C)",
            "pet": "pet_sim(mm/day)",
        },
    )
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=1045, gradient=-0.6
    )
    forcing.spatialize_from_station_data(variable="pet")
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=1045, gradient=0.0
    )
    model = models.Socont(
        soil_storage_nb=2, surface_runoff="linear_storage", channel_routing="lag"
    )
    parameters = model.generate_parameters()
    parameters.set_values(
        {
            "A": 458,
            "a_snow": 3,
            "k_slow_1": 0.9,
            "k_slow_2": 0.8,
            "k_quick": 1,
            "percol": 9.8,
            "channel_celerity": 1.0,
        }
    )
    out = tmp_path / "run"
    out.mkdir()
    model.setup(
        spatial_structure=units,
        output_path=str(out),
        start_date="1981-01-01",
        end_date="1981-03-31",
    )
    model.run(parameters=parameters, forcing=forcing)
    assert model.get_subbasin_ids() == [1, 2]
    assert np.all(model.get_subbasin_discharge(1) >= 0)
    assert model.get_total_outlet_discharge() > 0


def test_outlets_from_a_point_file(catchment, tmp_path):
    import geopandas as gpd
    from shapely.geometry import Point

    gdf = gpd.GeoDataFrame(
        {"name": ["Appenzell"]},
        geometry=[Point(*APPENZELL_GAUGE)],
        crs=catchment.crs,
    )
    path = tmp_path / "gauges.geojson"
    gdf.to_file(path, driver="GeoJSON")
    table = catchment.delineate_subbasins(path)
    assert list(table["name"]) == ["Appenzell", "outlet"]
    # The names given explicitly win over the file's.
    table = catchment.delineate_subbasins(path, names=["upper Sitter"])
    assert list(table["name"]) == ["upper Sitter", "outlet"]
    assert isinstance(table, pd.DataFrame)


def test_distance_along_a_river_network(catchment):
    """The graph helpers measure the distance along the lines, not as the crow flies."""
    import geopandas as gpd
    from shapely.geometry import LineString

    network = catchment.network
    lines = [
        LineString([(0, 0), (100, 0)]),  # the main stem, upstream part
        LineString([(100, 0), (100, 50)]),  # a tributary joining it
        LineString([(100, 0), (200, 0)]),  # the main stem, downstream part
        LineString([(500, 500), (600, 500)]),  # a disconnected piece
    ]
    graph = network._river_graph(gpd.GeoDataFrame(geometry=lines))

    # Across three lines, and along a single one.
    assert network._network_length(graph, (0, 0), (200, 0), 10) == pytest.approx(200)
    assert network._network_length(graph, (100, 50), (200, 0), 10) == pytest.approx(150)
    assert network._network_length(graph, (20, 0), (60, 0), 10) == pytest.approx(40)

    # A point too far from any line, and one on a disconnected piece.
    assert network._network_length(graph, (0, 500), (200, 0), 10) is None
    assert network._network_length(graph, (550, 500), (200, 0), 10) is None


def test_reach_length_from_the_river_network(catchment):
    """A mapped river is longer than the flow path, and the reach follows it."""
    import geopandas as gpd
    from shapely.geometry import LineString

    flow_path = catchment.delineate_subbasins([APPENZELL_GAUGE], names=["Appenzell"])
    upstream = flow_path[flow_path["name"] == "Appenzell"].iloc[0]
    downstream = flow_path[flow_path["name"] == "outlet"].iloc[0]

    # A synthetic river between the two outlets, meandering enough to be 20 % longer
    # than the flow path of the downstream reach.
    start = np.array([upstream["outlet_x"], upstream["outlet_y"]])
    end = np.array([downstream["outlet_x"], downstream["outlet_y"]])
    target = 1.2 * downstream["length"]

    def meander(amplitude, points=21):
        along = np.linspace(0, 1, points)
        axis = end - start
        normal = np.array([-axis[1], axis[0]])
        normal = normal / np.linalg.norm(normal)
        offsets = amplitude * np.sin(np.linspace(0, 4 * np.pi, points))
        return LineString(
            [start + t * axis + o * normal for t, o in zip(along, offsets)]
        )

    low, high = 0.0, float(np.linalg.norm(end - start))
    for _ in range(60):
        middle = 0.5 * (low + high)
        if meander(middle).length < target:
            low = middle
        else:
            high = middle
    river = meander(0.5 * (low + high))
    rivers = gpd.GeoDataFrame(geometry=[river], crs=catchment.crs)

    table = catchment.delineate_subbasins(
        [APPENZELL_GAUGE], names=["Appenzell"], river_network=rivers
    )
    routed = table[table["name"] == "outlet"].iloc[0]

    # The reach now follows the river, and is longer than the flow path.
    assert routed["length"] == pytest.approx(river.length, rel=0.02)
    assert routed["length"] > downstream["length"]

    # The slope is the drop over that length: the drop itself is unchanged, so the
    # product of the two is an invariant and the reach gets gentler, not shorter only.
    assert routed["slope"] < downstream["slope"]
    assert routed["slope"] * routed["length"] == pytest.approx(
        downstream["slope"] * downstream["length"], rel=0.02
    )

    # The headwater reach is not covered by the river: it keeps its flow path length.
    assert table[table["name"] == "Appenzell"].iloc[0]["length"] == pytest.approx(
        upstream["length"]
    )

    # And the flow path can be kept explicitly, river network or not.
    kept = catchment.delineate_subbasins(
        [APPENZELL_GAUGE],
        names=["Appenzell"],
        river_network=rivers,
        reach_lengths="flow_path",
    )
    assert kept[kept["name"] == "outlet"].iloc[0]["length"] == pytest.approx(
        downstream["length"]
    )

    with pytest.raises(hb.ConfigurationError):
        catchment.delineate_subbasins([APPENZELL_GAUGE], reach_lengths="straight")


def test_unit_raster_spanning_the_subbasins_is_refused():
    catchment = hb.Catchment(STGALLEN_DIR / "outline.shp")
    catchment.extract_dem(STGALLEN_DIR / "dem.tif")
    catchment.delineate_subbasins({"Appenzell": APPENZELL_GAUGE})
    # Elevation bands drawn without the subbasins cross the Appenzell outline.
    catchment.load_unit_ids_from_raster(STGALLEN_DIR / "unit_ids.tif")
    with pytest.raises(hb.DataError, match="span several subbasins"):
        catchment.get_hydro_units_attributes()


def _outlines(crs):
    """The official St. Gallen and Appenzell outlines (nested drained areas)."""
    import geopandas as gpd
    from shapely.ops import unary_union

    stgallen = gpd.read_file(STGALLEN_DIR / "outline.shp").to_crs(crs)
    appenzell = gpd.read_file(APPENZELL_DIR / "outline.shp").to_crs(crs)
    return unary_union(list(stgallen.geometry)), unary_union(list(appenzell.geometry))


def _polygons(crs, geometries, **columns):
    import geopandas as gpd

    return gpd.GeoDataFrame(columns, geometry=geometries, crs=crs)


def test_subbasins_from_nested_drained_areas(catchment):
    stgallen, appenzell = _outlines(catchment.crs)
    polygons = _polygons(
        catchment.crs, [stgallen, appenzell], name=["St. Gallen", "Appenzell"]
    )
    table = catchment.delineate_subbasins(subbasins=polygons)

    # The smaller area is carved out of the larger one and drains into it.
    assert list(table["name"]) == ["St. Gallen", "Appenzell"]
    assert list(table["downstream"]) == [0, 1]
    assert table.loc[1, "area_drained"] == pytest.approx(APPENZELL_AREA, rel=0.05)
    assert table.loc[0, "area_drained"] == pytest.approx(_area(catchment), rel=0.01)
    assert table["area_local"].sum() == pytest.approx(table.loc[0, "area_drained"])
    # The reaches are measured as for outlet points.
    assert table.loc[0, "length"] > 5000
    assert 0 < table.loc[0, "slope"] < 0.1
    assert table.loc[1, "length"] > 1000
    assert table.loc[1, "elevation_outlet"] > table.loc[0, "elevation_outlet"]
    # The outlet found on the DEM is the gauge.
    assert abs(table.loc[1, "outlet_x"] - APPENZELL_GAUGE[0]) < 500
    assert abs(table.loc[1, "outlet_y"] - APPENZELL_GAUGE[1]) < 500


def test_subbasins_from_a_partition_match_the_nested_areas(catchment):
    stgallen, appenzell = _outlines(catchment.crs)
    nested = catchment.delineate_subbasins(
        subbasins=_polygons(catchment.crs, [stgallen, appenzell])
    )
    partition = catchment.delineate_subbasins(
        subbasins=_polygons(
            catchment.crs,
            [appenzell, stgallen.difference(appenzell)],
            name=["Appenzell", "lower Sitter"],
        )
    )
    # The same subbasins, whether given as local or as drained areas.
    assert list(partition["downstream"]) == [2, 0]
    assert partition.loc[0, "area_local"] == pytest.approx(
        nested.loc[1, "area_local"], rel=1e-3
    )
    assert partition.loc[1, "area_drained"] == pytest.approx(
        nested.loc[0, "area_drained"], rel=1e-3
    )
    assert partition.loc[1, "length"] == pytest.approx(
        nested.loc[0, "length"], rel=0.05
    )


def test_uncovered_part_becomes_the_outlet_subbasin(catchment):
    _, appenzell = _outlines(catchment.crs)
    table = catchment.delineate_subbasins(
        subbasins=_polygons(catchment.crs, [appenzell]), names=["Appenzell"]
    )
    assert list(table["name"]) == ["Appenzell", "outlet"]
    assert list(table["downstream"]) == [2, 0]
    assert table.loc[1, "area_drained"] == pytest.approx(_area(catchment), rel=0.01)


def test_subbasin_topology_from_a_downstream_column(catchment):
    stgallen, appenzell = _outlines(catchment.crs)
    geometries = [appenzell, stgallen.difference(appenzell)]
    table = catchment.delineate_subbasins(
        subbasins=_polygons(catchment.crs, geometries, code=[10, 20], to=[20, 0]),
        id_column="code",
        downstream_column="to",
    )
    assert list(table["downstream"]) == [2, 0]
    assert list(table["source_id"]) == [10, 20]

    # Two subbasins draining out of the catchment do not make a tree.
    with pytest.raises(hb.DataError, match="2 outlets"):
        catchment.delineate_subbasins(
            subbasins=_polygons(catchment.crs, geometries, code=[10, 20], to=[0, 0]),
            id_column="code",
            downstream_column="to",
        )


def test_subbasins_before_the_hydro_units_without_flow_directions(monkeypatch):
    """Polygons and a river network need no flow directions, and the hydro units
    discretized afterwards each lie within a single subbasin."""
    from shapely.geometry import LineString

    from hydrobricks.preprocessing.catchment_network import CatchmentNetwork

    catchment = hb.Catchment(STGALLEN_DIR / "outline.shp")
    catchment.extract_dem(STGALLEN_DIR / "dem.tif")
    stgallen, appenzell = _outlines(catchment.crs)
    polygons = _polygons(catchment.crs, [stgallen, appenzell])
    reference = catchment.delineate_subbasins(subbasins=polygons)

    # A river network: the main stem between the two outlets, and a headwater stream
    # from inside the Appenzell subbasin down to its outlet.
    gauge = (reference.loc[1, "outlet_x"], reference.loc[1, "outlet_y"])
    outlet = (reference.loc[0, "outlet_x"], reference.loc[0, "outlet_y"])
    source = appenzell.representative_point().coords[0]
    main_stem = LineString([gauge, outlet])
    headwater = LineString([source, gauge])
    rivers = _polygons(catchment.crs, [main_stem, headwater])

    def no_flow_directions(self):
        pytest.fail("The flow directions were computed.")

    monkeypatch.setattr(CatchmentNetwork, "_flow_grid", no_flow_directions)
    catchment.network._grid = None
    partition = _polygons(catchment.crs, [appenzell, stgallen.difference(appenzell)])
    table = catchment.delineate_subbasins(subbasins=partition, river_network=rivers)
    assert list(table["downstream"]) == [2, 0]

    table = catchment.delineate_subbasins(subbasins=polygons, river_network=rivers)
    assert list(table["downstream"]) == [0, 1]
    assert list(table["area_local"]) == pytest.approx(list(reference["area_local"]))
    # The reaches follow the river lines.
    assert table.loc[0, "length"] == pytest.approx(main_stem.length, rel=0.05)
    assert table.loc[1, "length"] == pytest.approx(headwater.length, rel=0.05)
    assert catchment.map_unit_ids is None

    # The hydro units are created within the subbasins.
    catchment.create_elevation_bands(distance=200)
    hydro_units = catchment.hydro_units
    unit_ids = hydro_units.hydro_units["id"].iloc[:, 0].to_numpy()
    for unit_id in unit_ids:
        cells = catchment.map_subbasin_ids[catchment.map_unit_ids == unit_id]
        assert len(np.unique(cells)) == 1
    assert hydro_units.get_subbasin_ids() == [1, 2]
    areas = hydro_units.get_subbasin_areas()
    assert areas.loc[2, "local"] == pytest.approx(table.loc[1, "area_local"], rel=1e-3)
    assert areas.loc[1, "drained"] == pytest.approx(
        table.loc[0, "area_drained"], rel=1e-3
    )
