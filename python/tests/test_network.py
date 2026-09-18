"""A catchment split into subbasins runs as a river network.

With the instantaneous routing (the only scheme for now) and the linear stores of
Socont, the split catchment must reproduce the lumped one exactly (superposition),
and an upstream subbasin must reproduce a standalone run on its own units.
"""

import os.path
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

import hydrobricks as hb
import hydrobricks.models as models

TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    "..",
    "..",
    "tests",
    "files",
    "catchments",
)
STGALLEN_HUS = TEST_FILES_DIR / "ch_sitter_stgallen" / "elevation_bands.csv"
STGALLEN_METEO = TEST_FILES_DIR / "ch_sitter_stgallen" / "meteo.csv"
STGALLEN_ELEVATION = 1045

_START = "1981-01-01"
_END = "1982-12-31"
_UPSTREAM_MIN_ID = 21  # bands 21-40 (the higher ones) form the upstream subbasin


def _load_units(path: Path) -> hb.HydroUnits:
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(path)
    return hydro_units


def _split_units(hydro_units: hb.HydroUnits) -> None:
    ids = hydro_units.hydro_units["id"].iloc[:, 0].to_numpy()
    subbasin = np.where(ids >= _UPSTREAM_MIN_ID, 2, 1)
    hydro_units.add_property(("subbasin", "-"), subbasin)
    hydro_units.set_subbasins(
        pd.DataFrame(
            {
                "id": [1, 2],
                "downstream": [0, 1],
                "name": ["St. Gallen", "upper Sitter"],
            }
        )
    )


def _forcing(hydro_units: hb.HydroUnits) -> hb.Forcing:
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        STGALLEN_METEO,
        column_time="date",
        time_format="%d/%m/%Y",
        content={
            "precipitation": "precip(mm/day)",
            "temperature": "temp(C)",
            "pet": "pet_sim(mm/day)",
        },
    )
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=STGALLEN_ELEVATION, gradient=-0.6
    )
    forcing.spatialize_from_station_data(variable="pet")
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=STGALLEN_ELEVATION, gradient=0.05
    )
    return forcing


def _run(hydro_units: hb.HydroUnits, out: Path) -> models.Socont:
    model = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage")
    parameters = model.generate_parameters()
    # A large capacity keeps the slow reservoir linear (no overflow), which the
    # lumped-vs-split equivalence relies on.
    parameters.set_values(
        {
            "A": 2000,
            "a_snow": 3,
            "k_slow_1": 0.9,
            "k_slow_2": 0.8,
            "k_quick": 1,
            "percol": 9.8,
        }
    )
    out.mkdir(exist_ok=True)
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(out),
        start_date=_START,
        end_date=_END,
    )
    model.run(parameters=parameters, forcing=_forcing(hydro_units))
    return model


@pytest.fixture(scope="module")
def runs(tmp_path_factory) -> dict:
    base = tmp_path_factory.mktemp("network")

    lumped_units = _load_units(STGALLEN_HUS)
    lumped = _run(lumped_units, base / "lumped")

    split_units = _load_units(STGALLEN_HUS)
    _split_units(split_units)
    network = _run(split_units, base / "network")
    network.dump_outputs(str(base / "network"))

    # The upstream subbasin on its own: only the bands it holds.
    table = pd.read_csv(STGALLEN_HUS, header=[0, 1])
    upper = table[table[("id", "-")] >= _UPSTREAM_MIN_ID]
    upper_path = base / "upper_bands.csv"
    upper.to_csv(upper_path, index=False)
    upstream = _run(_load_units(upper_path), base / "upstream")

    return {
        "lumped": lumped,
        "network": network,
        "upstream": upstream,
        "results": base / "network" / "results.nc",
    }


def test_network_declares_two_subbasins(runs):
    network = runs["network"]
    assert network.get_subbasin_ids() == [2, 1]  # upstream first, outlet last
    assert network.get_subbasin_downstream_ids() == [1, 0]
    areas = network.get_subbasin_areas()
    assert areas.loc[2, "local"] == areas.loc[2, "drained"]
    assert areas.loc[1, "drained"] == pytest.approx(
        areas.loc[1, "local"] + areas.loc[2, "local"]
    )
    assert runs["lumped"].get_subbasin_ids() == [1]


def test_split_catchment_reproduces_lumped_outlet(runs):
    lumped = runs["lumped"].get_outlet_discharge()
    network = runs["network"].get_outlet_discharge()
    assert len(network) == len(lumped)
    np.testing.assert_allclose(network, lumped, rtol=1e-9, atol=1e-12)
    assert runs["network"].get_total_outlet_discharge() == pytest.approx(
        runs["lumped"].get_total_outlet_discharge(), rel=1e-9
    )
    assert runs["network"].get_total_et() == pytest.approx(
        runs["lumped"].get_total_et(), rel=1e-9
    )
    assert runs["network"].get_total_water_storage_changes() == pytest.approx(
        runs["lumped"].get_total_water_storage_changes(), rel=1e-9, abs=1e-9
    )


def test_upstream_subbasin_matches_standalone_run(runs):
    upstream = runs["network"].get_subbasin_discharge(2)
    alone = runs["upstream"].get_outlet_discharge()
    np.testing.assert_allclose(upstream, alone, rtol=1e-9, atol=1e-12)
    # The terminal subbasin's discharge is the outlet discharge.
    np.testing.assert_allclose(
        runs["network"].get_subbasin_discharge(1),
        runs["network"].get_outlet_discharge(),
        rtol=0,
        atol=0,
    )


def test_unknown_subbasin_raises(runs):
    with pytest.raises((RuntimeError, ValueError)):
        runs["network"].get_subbasin_discharge(7)


def test_results_file_has_the_subbasin_dimension(runs):
    if not hb.HAS_XARRAY:
        pytest.skip("xarray not installed")
    network = runs["network"]
    with hb.Results(str(runs["results"])) as results:
        assert list(results.subbasin_ids) == [2, 1]
        assert results.results["subbasin_values"].dims == (
            "aggregated_values",
            "subbasins",
            "time",
        )
        np.testing.assert_allclose(
            results.get_subbasin_values("outlet", 2),
            network.get_subbasin_discharge(2),
            rtol=1e-12,
        )
        np.testing.assert_allclose(
            results.get_subbasin_values("outlet"),
            network.get_outlet_discharge(),
            rtol=1e-12,
        )
        with pytest.raises(hb.DataError):
            results.get_subbasin_values("outlet", 7)
        with pytest.raises(hb.DataError):
            results.get_subbasin_values("no_such_component")
