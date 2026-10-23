import tempfile
from pathlib import Path

import numpy as np
import pytest

import hydrobricks as hb
from hydrobricks import soils
from hydrobricks._exceptions import ConfigurationError

FILES_DIR = Path(__file__).parent.parent.parent / "tests" / "files" / "catchments"
SITTER_OUTLINE = FILES_DIR / "ch_sitter_appenzell" / "outline.shp"
SITTER_DEM = FILES_DIR / "ch_sitter_appenzell" / "dem.tif"


def _write_raster(path, data, like=SITTER_DEM):
    """Write an array on the grid of an existing raster."""
    import rasterio

    with rasterio.open(like) as src:
        profile = src.profile
    profile.update(dtype="float32", count=1, nodata=None, tiled=False)
    for key in ("blockxsize", "blockysize"):
        profile.pop(key, None)
    with rasterio.open(path, "w", **profile) as dst:
        dst.write(np.broadcast_to(data, (profile["height"], profile["width"])), 1)
    return path


def _read(path):
    import rasterio

    with rasterio.open(path) as src:
        return src.read(1)


@pytest.mark.parametrize(
    "sand, clay, wilting_point, field_capacity, saturation, ksat",
    [
        # Table 3 of Saxton & Rawls (2006), at 2.5% organic matter.
        (88, 5, 5, 10, 46, 108.1),
        (40, 20, 14, 28, 46, 15.5),
        (25, 50, 30, 42, 50, 1.1),
    ],
)
def test_saxton_rawls_reproduces_the_paper(
    sand, clay, wilting_point, field_capacity, saturation, ksat
):
    values = soils.saxton_rawls(sand, clay, 2.5)
    assert values["wilting_point"] == pytest.approx(wilting_point, abs=0.5)
    assert values["field_capacity"] == pytest.approx(field_capacity, abs=0.5)
    assert values["saturation"] == pytest.approx(saturation, abs=0.5)
    assert values["ksat"] == pytest.approx(ksat, rel=0.05)
    assert values["awc"] == pytest.approx(
        values["field_capacity"] - values["wilting_point"]
    )


def test_saxton_rawls_on_arrays():
    values = soils.saxton_rawls(np.array([88.0, 25.0]), np.array([5.0, 50.0]))
    assert values["ksat"].shape == (2,)
    assert values["ksat"][0] > values["ksat"][1]


def test_layer_weights_follow_the_thickness_within_the_depth():
    depths = [(0, 0.05), (0.05, 0.15), (0.15, 0.3), (0.3, 0.6)]
    assert soils.layer_weights(depths) * 0.6 == pytest.approx([0.05, 0.1, 0.15, 0.3])
    # Down to 0.2 m: the third layer counts for 5 cm, the last not at all.
    assert soils.layer_weights(depths, 0.2) == pytest.approx([0.25, 0.5, 0.25, 0])


def test_layer_weights_refuse_invalid_depths():
    with pytest.raises(ConfigurationError):
        soils.layer_weights([(0.1, 0.05)])
    with pytest.raises(ConfigurationError):
        soils.layer_weights([(0.5, 1.0)], max_depth=0.2)


def test_integrate_layers():
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_dir = Path(tmp_dir)
        layers = [
            _write_raster(tmp_dir / f"layer_{i}.tif", np.float32(value))
            for i, value in enumerate([0.30, 0.20, 0.10])
        ]
        path = soils.integrate_layers(
            layers,
            [(0, 0.1), (0.1, 0.3), (0.3, 1.0)],
            tmp_dir / "mean.tif",
            max_depth=0.5,
            scale=100,
        )
        # (0.1 * 30 + 0.2 * 20 + 0.2 * 10) / 0.5
        assert np.all(_read(path) == pytest.approx(18.0))


def test_hydraulics_from_texture():
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_dir = Path(tmp_dir)
        # SoilGrids-like g/kg.
        sand = _write_raster(tmp_dir / "sand.tif", np.float32(400))
        clay = _write_raster(tmp_dir / "clay.tif", np.float32(200))
        paths = soils.hydraulics_from_texture(
            sand, clay, tmp_dir / "out", scale={"sand": 0.1, "clay": 0.1}
        )
        expected = soils.saxton_rawls(40, 20)
        assert np.all(_read(paths["awc"]) == pytest.approx(expected["awc"], rel=1e-5))
        assert np.all(_read(paths["ksat"]) == pytest.approx(expected["ksat"], rel=1e-5))


def test_class_table_from_csv_weights_the_soils_of_a_unit():
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = Path(tmp_dir) / "hwsd.csv"
        path.write_text(
            "HWSD2_SMU_ID,SHARE,AWC\n"
            "10,60,0.10\n"
            "10,40,0.20\n"
            "11,100,0.15\n"
            "12,100,\n"
        )
        table = soils.class_table_from_csv(
            path, "HWSD2_SMU_ID", "AWC", weight_column="SHARE", scale=100
        )
        assert table == pytest.approx({10: 14.0, 11: 15.0})
        with pytest.raises(ConfigurationError):
            soils.class_table_from_csv(path, "HWSD2_SMU_ID", "KSAT")


def test_unknown_soil_map_is_refused():
    with pytest.raises(ConfigurationError):
        soils.get_soil_class_tables("atlantis")


def test_extract_soil_properties_units():
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_dir = Path(tmp_dir)
        catchment = hb.Catchment(SITTER_OUTLINE)
        catchment.extract_dem(SITTER_DEM)
        catchment.create_elevation_bands(method="equal_intervals", distance=100)
        k_values = _write_raster(tmp_dir / "ksat.tif", np.float32(12.0))
        k_classes = _write_raster(tmp_dir / "k.tif", np.float32(3))

        catchment.extract_soil_properties(conductivity=k_values)
        assert catchment.hydro_units.hydro_units["conductivity"].columns[0] == "mm/h"

        catchment.extract_soil_properties(
            conductivity=k_classes, reclassify="swiss_soil_suitability"
        )
        units = catchment.hydro_units.hydro_units
        assert units["conductivity"].columns[0] == "-"
        assert np.all(units["conductivity"].values == pytest.approx(0.5))
