import os.path
import tempfile
import uuid
from pathlib import Path

import numpy as np
import pytest

import hydrobricks as hb
import hydrobricks.models as models
from hydrobricks._area_basis import convert_area_basis, land_cover_of_label
from hydrobricks._exceptions import ConfigurationError, DataError

GLETSCH_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
    "ch_rhone_gletsch",
)


# --------------------------------------------------------------------------- #
# Conversion helpers (no model needed)
# --------------------------------------------------------------------------- #
def test_land_cover_of_label():
    covers = ["open", "glacier"]
    assert land_cover_of_label("glacier:melt:output", covers) == "glacier"
    assert land_cover_of_label("glacier_snowpack:snow_content", covers) == "glacier"
    assert land_cover_of_label("open_canopy:water_content", covers) == "open"
    assert land_cover_of_label("slow_reservoir:water_content", covers) is None


def test_convert_area_basis_round_trips():
    values = np.array([[10.0, 20.0], [4.0, 8.0]])  # (units, time)
    fraction = np.array([[0.5, 0.5], [0.25, 0.0]])
    subbasin_fraction = np.array([0.2, 0.8])

    # Over the land cover -> over the unit: weighted by the cover fraction.
    unit = convert_area_basis(
        values, "x", "land_cover", "hydro_unit", fraction, subbasin_fraction
    )
    assert np.allclose(unit, [[5.0, 10.0], [1.0, 0.0]])

    # And back, NaN where the cover is absent.
    back = convert_area_basis(
        unit, "x", "hydro_unit", "land_cover", fraction, subbasin_fraction
    )
    assert np.allclose(back[:, 0], values[:, 0])
    assert np.isnan(back[1, 1])

    # Over the subbasin -> over the unit: divided by the unit's share of it.
    unit = convert_area_basis(
        values, "x", "subbasin", "hydro_unit", None, subbasin_fraction
    )
    assert np.allclose(unit, [[50.0, 100.0], [5.0, 10.0]])


def test_convert_area_basis_errors():
    values = np.ones((2, 3))
    with pytest.raises(ConfigurationError):
        convert_area_basis(values, "x", "hydro_unit", "basin", None, np.ones(2))
    with pytest.raises(DataError):
        convert_area_basis(values, "x", "hydro_unit", "land_cover", None, np.ones(2))


# --------------------------------------------------------------------------- #
# Model-based tests (a small glacierized run, reused across tests)
# --------------------------------------------------------------------------- #
@pytest.fixture(scope="module")
def glacier_run():
    """Set up and run a small static-glacier SOCONT model on the Rhone catchment."""
    pytest.importorskip("rasterio")
    work_dir = Path(tempfile.gettempdir()) / f"hb_basistest_{uuid.uuid4().hex}"
    work_dir.mkdir(parents=True, exist_ok=True)

    catchment = hb.Catchment(
        GLETSCH_DIR / "outline.shp",
        land_cover_types=["open", "glacier"],
        land_cover_names=["open", "glacier"],
    )
    catchment.extract_dem(GLETSCH_DIR / "dem.tif")
    catchment.create_elevation_bands(method="equal_intervals", distance=200)

    ge = hb.preprocessing.GlacierEvolutionDeltaH()
    ge.compute_initial_ice_thickness(
        catchment, ice_thickness=GLETSCH_DIR / "glaciers" / "ice_thickness.tif"
    )

    model = models.Socont(
        soil_storage_nb=2,
        surface_runoff="linear_storage",
        record_all=True,
        land_cover_types=["open", "glacier"],
        land_cover_names=["open", "glacier"],
    )
    params = model.generate_parameters()
    params.set_values(
        {
            "A": 458,
            "a_snow": 4,
            "a_ice": 8,
            "k_slow_1": 0.9,
            "k_slow_2": 0.8,
            "k_quick": 1,
            "percol": 9.8,
            "k_snow": 0.1,
            "k_ice": 0.5,
        }
    )

    forcing = hb.Forcing(catchment.hydro_units)
    forcing.load_station_data_from_csv(
        GLETSCH_DIR / "meteo.csv",
        column_time="date",
        time_format="%d/%m/%Y",
        content={"precipitation": "precip(mm/day)", "temperature": "temp(C)"},
    )
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=2702, gradient=-0.6
    )
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=2702, gradient=0.05
    )
    forcing.compute_pet(method="Oudin", use=["t", "lat"], lat=46.6)

    model.setup(
        spatial_structure=catchment.hydro_units,
        output_path=str(work_dir),
        start_date="2015-01-01",
        end_date="2015-12-31",
    )
    model.run(parameters=params, forcing=forcing)
    model.dump_outputs(str(work_dir))
    results = hb.Results(str(work_dir / "results.nc"))
    return model, results


def test_recorded_bases(glacier_run):
    model, results = glacier_run
    expected = {
        "glacier_snowpack:snow_content": "land_cover",
        "slow_reservoir:water_content": "hydro_unit",
        "slow_reservoir:percolation:output": "hydro_unit",
        "slow_reservoir:outflow:output": "subbasin",
        "slow_reservoir_2:outflow:output": "subbasin",
    }
    for label, basis in expected.items():
        assert model.get_recorded_hydro_unit_basis(label) == basis
        assert results.get_hydro_units_basis(label) == basis


def test_unrequested_basis_returns_recorded_values(glacier_run):
    model, _ = glacier_run
    label = "slow_reservoir:outflow:output"
    recorded = model.get_recorded_hydro_unit_values(label)
    same = model.get_recorded_hydro_unit_values(label, basis="subbasin")
    assert np.allclose(recorded, same, equal_nan=True)


def test_subbasin_flux_over_units_keeps_the_subbasin_total(glacier_run):
    """Converted to mm over each unit, a flux to the outlet still sums to the
    same subbasin total once weighted by the unit areas."""
    model, _ = glacier_run
    label = "slow_reservoir:outflow:output"
    areas = model.get_recorded_hydro_unit_areas()
    recorded = model.get_recorded_hydro_unit_values(label)
    over_unit = model.get_recorded_hydro_unit_values(label, basis="hydro_unit")
    total_recorded = np.nansum(recorded, axis=0)
    total_over_unit = np.nansum(over_unit * areas[:, None], axis=0) / areas.sum()
    assert np.allclose(total_recorded, total_over_unit)
    # A unit smaller than the subbasin gets a larger depth over its own area.
    assert np.nanmax(over_unit) > np.nanmax(recorded)


def test_land_cover_state_over_unit(glacier_run):
    model, _ = glacier_run
    label = "glacier_snowpack:snow_content"
    swe = model.get_recorded_hydro_unit_values(label)
    fraction = model.get_recorded_hydro_unit_fractions("glacier")
    over_unit = model.get_recorded_hydro_unit_values(label, basis="hydro_unit")
    assert np.allclose(
        np.nan_to_num(over_unit), np.nan_to_num(swe) * np.nan_to_num(fraction)
    )


def test_unit_brick_cannot_be_expressed_over_a_land_cover(glacier_run):
    model, _ = glacier_run
    with pytest.raises(DataError):
        model.get_recorded_hydro_unit_values(
            "slow_reservoir:water_content", basis="land_cover"
        )


def test_netcdf_conversion_matches_memory(glacier_run):
    model, results = glacier_run
    for label in ("glacier_snowpack:snow_content", "slow_reservoir:outflow:output"):
        mem = model.get_recorded_hydro_unit_values(label, basis="hydro_unit")
        nc = results.get_hydro_units_values(label, basis="hydro_unit")
        assert np.allclose(np.nan_to_num(mem), np.nan_to_num(nc), atol=1e-9)
