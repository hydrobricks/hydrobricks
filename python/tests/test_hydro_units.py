import os
import tempfile
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

import hydrobricks as hb

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
)

RHONE_HUS = (
    TEST_FILES_DIR
    / "catchments"
    / "ch_rhone_gletsch"
    / "hydro_units_elevation_radiation.csv"
)
RHONE_CONNECT = (
    TEST_FILES_DIR
    / "catchments"
    / "ch_rhone_gletsch"
    / "connectivity_elevation_radiation.csv"
)


def test_hydro_units_creation():
    hb.HydroUnits()


def test_hydro_units_creation_with_land_covers():
    hb.HydroUnits(
        land_cover_types=["ground", "glacier"], land_cover_names=["ground", "glacier"]
    )


def test_hydro_units_default_cover_is_open():
    assert hb.HydroUnits().land_cover_names == ["open"]
    assert hb.HydroUnits().land_cover_types == ["open"]


def test_generic_cover_name_resolution():
    """The generic (residual) soil cover defaults to 'open' but recognises the 'ground'
    alias for backward compatibility, and is found alongside a glacier cover."""
    assert hb.HydroUnits().get_generic_cover_name() == "open"
    assert (
        hb.HydroUnits(
            land_cover_types=["open", "glacier"], land_cover_names=["open", "glacier"]
        ).get_generic_cover_name()
        == "open"
    )
    assert (
        hb.HydroUnits(
            land_cover_types=["ground", "glacier"],
            land_cover_names=["ground", "glacier"],
        ).get_generic_cover_name()
        == "ground"
    )


def test_hydro_units_creation_with_land_covers_mismatch():
    with pytest.raises(hb.DataError):
        hb.HydroUnits(
            land_cover_types=["ground", "glacier", "glacier"], land_cover_names=None
        )


def test_hydro_units_creation_with_land_covers_size_mismatch():
    with pytest.raises(hb.DataError):
        hb.HydroUnits(
            land_cover_types=["ground", "glacier", "glacier"],
            land_cover_names=["ground", "glacier"],
        )


@pytest.fixture
def hydro_units():
    hydro_units = hb.HydroUnits(
        land_cover_types=["ground", "glacier", "glacier"],
        land_cover_names=["ground", "glacier_ice", "glacier_debris"],
    )
    return hydro_units


@pytest.fixture
def hydro_units_csv(hydro_units: hb.HydroUnits):
    hydro_units.load_from_csv(
        TEST_FILES_DIR / "parsing" / "hydro_units_absolute_areas.csv",
        column_elevation="Elevation Bands",
        columns_areas={
            "ground": "Sum_Area Non Glacier Band",
            "glacier_ice": "Sum_Area ICE Band",
            "glacier_debris": "Sum_Area Debris Band",
        },
    )
    return hydro_units


def test_load_from_csv(hydro_units_csv: hb.HydroUnits):
    hu = hydro_units_csv.hydro_units
    assert hu.loc[0].at["id"].values == 1
    assert hu.loc[10].at["id"].values == 11
    assert hu.loc[20].at["id"].values == 21
    assert hu.loc[0].at["area"].values == pytest.approx(2408000, abs=0.001)
    assert hu.loc[10].at["area"].values == pytest.approx(2806000, abs=0.001)
    assert hu.loc[20].at["area"].values == pytest.approx(1483000, abs=0.001)
    assert hu.loc[0].at["elevation"].values == 3986
    assert hu.loc[10].at["elevation"].values == 4346
    assert hu.loc[20].at["elevation"].values == 4706
    assert hu.loc[0].at["fraction-ground"].values == 1
    assert hu.loc[10].at["fraction-ground"].values == pytest.approx(0.918, abs=0.001)
    assert hu.loc[20].at["fraction-ground"].values == pytest.approx(0.770, abs=0.001)
    fg = "fraction-glacier"
    assert hu.loc[0].at[f"{fg}_ice"].values == 0
    assert hu.loc[10].at[f"{fg}_ice"].values == pytest.approx(0.018, abs=0.001)
    assert hu.loc[20].at[f"{fg}_ice"].values == pytest.approx(0.206, abs=0.001)
    assert hu.loc[0].at[f"{fg}_debris"].values == 0
    assert hu.loc[10].at[f"{fg}_debris"].values == pytest.approx(0.062, abs=0.001)
    assert hu.loc[20].at[f"{fg}_debris"].values == pytest.approx(0.023, abs=0.001)


def test_create_file(hydro_units_csv: hb.HydroUnits):
    if not hb.HAS_NETCDF:
        return

    with tempfile.TemporaryDirectory() as tmp_dir:
        hydro_units_csv.save_as(tmp_dir + "/test.nc")


def test_set_connectivity():
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(RHONE_HUS)
    hydro_units.set_connectivity(RHONE_CONNECT)

    assert hydro_units.settings.get_lateral_connection_count() == 359


def test_initialize_from_land_cover_change_double_application_raises():
    """Driving the generic cover negative (e.g. initializing a cover twice) raises a
    clear error rather than a bare assertion deeper in the build."""
    units = hb.HydroUnits(
        land_cover_types=["open", "glacier"], land_cover_names=["open", "glacier"]
    )
    units.load_from_csv(RHONE_HUS)

    unit_id = int(units.hydro_units.loc[0].at["id"].values[0])
    unit_area = float(units.hydro_units.loc[0].at["area"].values[0])

    # 60% of the unit -> glacier 0.6, open 0.4.
    change = pd.DataFrame({"hydro_unit": [unit_id], "area": [0.6 * unit_area]})
    units.initialize_from_land_cover_change("glacier", change)

    # Applying it again would drive the generic 'open' cover negative (-0.2).
    with pytest.raises(hb.DataError, match="initialize_cover"):
        units.initialize_from_land_cover_change("glacier", change)


# ---- River network (subbasins) ------------------------------------------------


def _subbasin_table() -> pd.DataFrame:
    return pd.DataFrame(
        {
            "id": [1, 2],
            "downstream": [0, 1],
            "name": ["gauge_down", "gauge_up"],
            "length": [1500.0, 2500.0],
        }
    )


def _assign_two_subbasins(hydro_units: hb.HydroUnits) -> None:
    n = hydro_units.get_hydro_unit_count()
    subbasin = np.ones(n, dtype=int)
    subbasin[: n // 2] = 2  # the upper half of the bands drains to subbasin 2
    hydro_units.add_property(("subbasin", "-"), subbasin)


def test_single_subbasin_by_default(hydro_units_csv: hb.HydroUnits):
    assert hydro_units_csv.subbasins is None
    assert hydro_units_csv.get_subbasin_ids() == [1]
    assert hydro_units_csv.settings.get_subbasin_count() == 0
    ids = hydro_units_csv.settings.get_hydro_unit_subbasin_ids()
    assert len(ids) == hydro_units_csv.get_hydro_unit_count()
    assert set(ids) == {1}
    hydro_units_csv.settings.validate_network()


def test_set_subbasins_reaches_the_settings(hydro_units_csv: hb.HydroUnits):
    _assign_two_subbasins(hydro_units_csv)
    hydro_units_csv.set_subbasins(_subbasin_table())

    assert hydro_units_csv.get_subbasin_ids() == [1, 2]
    settings = hydro_units_csv.settings
    assert settings.get_subbasin_count() == 2
    assert settings.get_subbasin_ids() == [1, 2]
    assert settings.get_subbasin_downstream_ids() == [0, 1]
    assert settings.get_subbasin_property_double(1, "length") == 2500.0
    unit_ids = settings.get_hydro_unit_subbasin_ids()
    n = hydro_units_csv.get_hydro_unit_count()
    assert unit_ids.count(2) == n // 2
    assert unit_ids.count(1) == n - n // 2
    settings.validate_network()


def test_set_subbasins_from_csv(hydro_units_csv: hb.HydroUnits):
    _assign_two_subbasins(hydro_units_csv)
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = Path(tmp_dir) / "subbasins.csv"
        _subbasin_table().to_csv(path, index=False)
        hydro_units_csv.set_subbasins(path)
    assert hydro_units_csv.settings.get_subbasin_count() == 2
    assert list(hydro_units_csv.subbasins["name"]) == ["gauge_down", "gauge_up"]


def test_set_subbasins_needs_id_and_downstream(hydro_units_csv: hb.HydroUnits):
    with pytest.raises(hb.DataError):
        hydro_units_csv.set_subbasins(pd.DataFrame({"id": [1, 2]}))
    with pytest.raises(hb.DataError):
        hydro_units_csv.set_subbasins(
            pd.DataFrame({"id": [1, 1], "downstream": [0, 1]})
        )


def test_set_subbasins_invalid_tree_raises(hydro_units_csv: hb.HydroUnits):
    _assign_two_subbasins(hydro_units_csv)
    table = _subbasin_table()
    table.loc[1, "downstream"] = 5  # undeclared subbasin
    with pytest.raises(hb.DataError, match="undeclared subbasin 5"):
        hydro_units_csv.set_subbasins(table)


def test_set_subbasins_units_must_be_on_declared_subbasins(
    hydro_units_csv: hb.HydroUnits,
):
    _assign_two_subbasins(hydro_units_csv)
    table = _subbasin_table().iloc[[0]]  # only subbasin 1 declared
    with pytest.raises(hb.DataError, match="undeclared subbasin 2"):
        hydro_units_csv.set_subbasins(table)


def test_subbasin_column_without_table_is_refused_at_setup(
    hydro_units_csv: hb.HydroUnits,
):
    """A 'subbasin' column alone is fine to load (the table may come later), but a
    model cannot be set up on it: the core refuses the undeclared network."""
    _assign_two_subbasins(hydro_units_csv)
    with pytest.raises(ValueError, match="no subbasin is declared"):
        hydro_units_csv.settings.validate_network()


def test_declared_single_subbasin_model_sets_up(tmp_path: Path):
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        TEST_FILES_DIR
        / "catchments"
        / "ch_sitter_appenzell"
        / "hydro_units_elevation.csv"
    )
    hydro_units.set_subbasins(
        pd.DataFrame({"id": [1], "downstream": [0], "name": ["outlet"]})
    )
    model = hb.models.Socont(surface_runoff="linear_storage")
    model.setup(
        spatial_structure=hydro_units,
        output_path=tmp_path,
        start_date="2020-01-01",
        end_date="2020-01-10",
    )


def test_save_as_round_trips_the_network(hydro_units_csv: hb.HydroUnits):
    if not hb.HAS_NETCDF:
        return
    from hydrobricks._hydrobricks import SettingsBasin

    _assign_two_subbasins(hydro_units_csv)
    hydro_units_csv.set_subbasins(_subbasin_table())
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = tmp_dir + "/units.nc"
        hydro_units_csv.save_as(path)
        parsed = SettingsBasin()
        assert parsed.parse(path)

    n = hydro_units_csv.get_hydro_unit_count()
    assert parsed.get_hydro_unit_count() == n
    assert parsed.get_subbasin_count() == 2
    assert parsed.get_subbasin_ids() == [1, 2]
    assert parsed.get_subbasin_downstream_ids() == [0, 1]
    assert parsed.get_subbasin_property_double(0, "length") == 1500.0
    assert sorted(parsed.get_hydro_unit_subbasin_ids()) == sorted(
        hydro_units_csv.settings.get_hydro_unit_subbasin_ids()
    )
    parsed.validate_network()


def test_save_as_without_network_parses_as_single_subbasin(
    hydro_units_csv: hb.HydroUnits,
):
    if not hb.HAS_NETCDF:
        return
    from hydrobricks._hydrobricks import SettingsBasin

    with tempfile.TemporaryDirectory() as tmp_dir:
        path = tmp_dir + "/units.nc"
        hydro_units_csv.save_as(path)
        parsed = SettingsBasin()
        assert parsed.parse(path)
    assert parsed.get_subbasin_count() == 0
    assert set(parsed.get_hydro_unit_subbasin_ids()) == {1}


def test_load_from_csv_picks_up_the_subbasin_column():
    source = hb.HydroUnits()
    source.load_from_csv(
        TEST_FILES_DIR
        / "catchments"
        / "ch_sitter_appenzell"
        / "hydro_units_elevation.csv"
    )
    _assign_two_subbasins(source)
    with tempfile.TemporaryDirectory() as tmp_dir:
        path = Path(tmp_dir) / "units.csv"
        source.save_to_csv(path)
        reloaded = hb.HydroUnits()
        reloaded.load_from_csv(path)
    assert reloaded.has("subbasin")
    assert reloaded.get_subbasin_ids() == [1, 2]
    reloaded.set_subbasins(pd.DataFrame({"id": [1, 2], "downstream": [0, 1]}))
    assert reloaded.settings.get_subbasin_count() == 2
