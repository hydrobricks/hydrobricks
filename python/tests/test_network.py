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


# ---- Routing schemes ----------------------------------------------------------------


def _split_units_with_reaches(length_m: float) -> hb.HydroUnits:
    hydro_units = _load_units(STGALLEN_HUS)
    ids = hydro_units.hydro_units["id"].iloc[:, 0].to_numpy()
    hydro_units.add_property(("subbasin", "-"), np.where(ids >= _UPSTREAM_MIN_ID, 2, 1))
    hydro_units.set_subbasins(
        pd.DataFrame(
            {
                "id": [1, 2],
                "downstream": [0, 1],
                "length": [
                    length_m,
                    3000.0,
                ],  # the outlet reach carries the upper Sitter
            }
        )
    )
    return hydro_units


def _run_routed(out: Path, routing: str, **params) -> models.Socont:
    hydro_units = _split_units_with_reaches(86400.0)
    model = models.Socont(
        soil_storage_nb=2,
        surface_runoff="linear_storage",
        channel_routing=routing,
        record_all=True,
    )
    parameters = model.generate_parameters()
    parameters.set_values(
        {
            "A": 2000,
            "a_snow": 3,
            "k_slow_1": 0.9,
            "k_slow_2": 0.8,
            "k_quick": 1,
            "percol": 9.8,
            **params,
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
    model.dump_outputs(str(out))
    return model


def test_routing_parameters_follow_the_scheme(tmp_path):
    assert (
        not models.Socont(surface_runoff="linear_storage")
        .generate_parameters()
        .has("channel_celerity")
    )
    lag = models.Socont(surface_runoff="linear_storage", channel_routing="lag")
    lag_params = lag.generate_parameters()
    assert lag_params.has("channel_celerity")
    assert not lag_params.has("muskingum_x")
    musk = models.Socont(surface_runoff="linear_storage", channel_routing="muskingum")
    musk_params = musk.generate_parameters()
    assert musk_params.has("channel_celerity")
    assert musk_params.has("muskingum_x")
    with pytest.raises(hb.ConfigurationError):
        models.Socont(
            surface_runoff="linear_storage", channel_routing="kinematic"
        ).setup(
            spatial_structure=_split_units_with_reaches(1000.0),
            output_path=str(tmp_path),
            start_date=_START,
            end_date=_END,
        )


def test_lag_routing_delays_the_upstream_water(runs, tmp_path):
    # 86.4 km at 1 m/s: the upper Sitter reaches the outlet one day later.
    lag = _run_routed(tmp_path / "lag", "lag", channel_celerity=1.0)
    none = runs["network"]
    areas = lag.get_subbasin_areas()
    ratio = areas.loc[2, "local"] / areas.loc[1, "drained"]

    np.testing.assert_allclose(
        lag.get_subbasin_discharge(2), none.get_subbasin_discharge(2), rtol=1e-9
    )
    up = none.get_subbasin_discharge(2)
    shifted = np.concatenate(([0.0], up[:-1]))
    expected = none.get_outlet_discharge() + (shifted - up) * ratio
    np.testing.assert_allclose(
        lag.get_outlet_discharge(), expected, rtol=1e-9, atol=1e-12
    )

    with hb.Results(str(tmp_path / "lag" / "results.nc")) as results:
        inflow = results.get_subbasin_values("reach:inflow", 1)
        outflow = results.get_subbasin_values("reach:outflow", 1)
        np.testing.assert_allclose(inflow, up * ratio, rtol=1e-9, atol=1e-12)
        np.testing.assert_allclose(outflow, shifted * ratio, rtol=1e-9, atol=1e-12)
        storage = results.get_subbasin_values("reach:storage", 1)
        assert np.all(storage >= 0)
        # The headwater's reach carries nothing.
        assert results.get_subbasin_values("reach:inflow", 2).sum() == 0.0


def test_muskingum_routing_conserves_mass_and_attenuates(runs, tmp_path):
    musk = _run_routed(
        tmp_path / "muskingum", "muskingum", channel_celerity=0.5, muskingum_x=0.2
    )
    none = runs["network"]
    np.testing.assert_allclose(
        musk.get_subbasin_discharge(2), none.get_subbasin_discharge(2), rtol=1e-9
    )
    with hb.Results(str(tmp_path / "muskingum" / "results.nc")) as results:
        storage = results.get_subbasin_values("reach:storage", 1)
    # Water balance at the outlet: routed totals differ from the instantaneous ones only
    # by what is still in the reach at the end.
    assert musk.get_total_outlet_discharge() + storage[-1] == pytest.approx(
        none.get_total_outlet_discharge(), rel=1e-9
    )
    # Attenuation: the routed outlet has a lower peak than the instantaneous one.
    assert musk.get_outlet_discharge().max() < none.get_outlet_discharge().max()
    assert np.all(musk.get_outlet_discharge() >= 0)


# ---- Gauges on subbasins ----------------------------------------------------------


def _upstream_gauge(network: models.Socont, **kwargs) -> hb.DischargeObservations:
    """A synthetic gauge on subbasin 2: the model's own upstream discharge plus noise,
    written to a CSV and loaded back like an observed record."""
    rng = np.random.default_rng(0)
    values = network.get_subbasin_discharge(2) * (1 + 0.1 * rng.standard_normal(1))
    values = np.maximum(values + 0.05 * rng.standard_normal(len(values)), 0.0)
    time = network.get_recorded_time()
    return values, time


def _write_gauge(path: Path, time, values, scale: float = 1.0) -> None:
    pd.DataFrame(
        {"date": time.strftime("%Y-%m-%d"), "q": np.asarray(values) * scale}
    ).to_csv(path, index=False)


def test_gauge_on_a_subbasin_is_scored_against_its_outlet(runs, tmp_path):
    network = runs["network"]
    values, time = _upstream_gauge(network)
    _write_gauge(tmp_path / "gauge.csv", time, values)

    gauge = hb.DischargeObservations(_START, _END, subbasin=2)
    gauge.load_from_csv(tmp_path / "gauge.csv", "date", "%Y-%m-%d", {"discharge": "q"})
    assert gauge.subbasin == 2
    assert gauge.name == "discharge (subbasin 2)"
    np.testing.assert_allclose(
        gauge.simulated_series(network), network.get_subbasin_discharge(2)
    )
    np.testing.assert_allclose(
        gauge.simulated(network), gauge.simulated_series(network)
    )

    # Scored against the subbasin outlet, not the catchment outlet.
    score_gauge = network.eval("nse", gauge)
    score_outlet = hb.evaluate(network.get_outlet_discharge(), gauge.observed(), "nse")
    assert score_gauge > score_outlet
    assert score_gauge > 0.9

    periods = hb.Periods(
        calibration=(_START, "1981-12-31"), validation=("1982-01-01", _END)
    )
    table = hb.evaluate_periods(network, gauge, periods, metrics=("nse",))
    assert table.loc["calibration", "nse"] > 0.9
    assert table.loc["validation", "nse"] > 0.9

    # An unknown units string and m3/s without an area are refused.
    with pytest.raises(hb.DataError):
        hb.DischargeObservations(_START, _END, units="liters")
    with pytest.raises(hb.DataError):
        hb.DischargeObservations(_START, _END, units="m3/s")


def test_gauge_in_cubic_meters_per_second_is_converted(runs, tmp_path):
    network = runs["network"]
    values, time = _upstream_gauge(network)
    areas = network.get_subbasin_areas()
    drained = float(areas.loc[2, "drained"])
    # mm/day -> m3/s: q [mm/d] * A [m2] / 1000 / 86400
    _write_gauge(
        tmp_path / "gauge_m3s.csv", time, values, scale=drained / 1000.0 / 86400.0
    )

    gauge = hb.DischargeObservations(
        _START, _END, subbasin=2, units="m3/s", drained_area=drained
    )
    gauge.load_from_csv(
        tmp_path / "gauge_m3s.csv", "date", "%Y-%m-%d", {"discharge": "q"}
    )
    np.testing.assert_allclose(gauge.observed(), values, rtol=1e-10)

    # The Python-side drained areas match the model's.
    hydro_units = _split_units_with_reaches(1000.0)
    table = hydro_units.get_subbasin_areas()
    assert table.loc[1, "drained"] == pytest.approx(areas.loc[1, "drained"])
    assert table.loc[2, "drained"] == pytest.approx(areas.loc[2, "drained"])
    assert table.loc[2, "local"] == pytest.approx(areas.loc[2, "local"])


def test_calibration_with_a_gauge_on_a_subbasin(runs, tmp_path):
    pytest.importorskip("spotpy")
    import hydrobricks.trainer as trainer

    network = runs["network"]
    values, time = _upstream_gauge(network)
    _write_gauge(tmp_path / "gauge.csv", time, values)
    outlet_values = network.get_outlet_discharge()
    _write_gauge(tmp_path / "outlet.csv", time, outlet_values)

    # A fresh model over the same period, calibrated on the outlet plus the
    # upstream gauge as an additional signal.
    hydro_units = _split_units_with_reaches(1000.0)
    model = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage")
    parameters = model.generate_parameters()
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
    parameters.allow_changing = ["a_snow", "k_quick"]
    out = tmp_path / "calib"
    out.mkdir()
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(out),
        start_date=_START,
        end_date=_END,
    )
    forcing = _forcing(hydro_units)

    outlet = hb.DischargeObservations(_START, _END)
    outlet.load_from_csv(
        tmp_path / "outlet.csv", "date", "%Y-%m-%d", {"discharge": "q"}
    )
    gauge = hb.DischargeObservations(_START, _END, subbasin=2, metric="nse", weight=0.5)
    gauge.load_from_csv(tmp_path / "gauge.csv", "date", "%Y-%m-%d", {"discharge": "q"})

    spot_setup = trainer.SpotpySetup(
        model,
        parameters,
        forcing,
        outlet,
        warmup=30,
        obj_func="nse",
        extra_observations=[gauge],
    )
    assert spot_setup._obs_subbasins == [None]
    sampler = trainer.calibrate(spot_setup, "mc", repetitions=3, dbformat="ram")
    best = trainer.get_best(sampler)
    assert np.isfinite(best["score"])

    # The primary signal may itself be the upstream gauge (a fresh object: the setup
    # above restricted `gauge` to its post-warmup evaluation period).
    gauge = hb.DischargeObservations(_START, _END, subbasin=2)
    gauge.load_from_csv(tmp_path / "gauge.csv", "date", "%Y-%m-%d", {"discharge": "q"})
    upstream_only = trainer.SpotpySetup(
        model, parameters, forcing, gauge, warmup=30, obj_func="nse"
    )
    assert upstream_only._obs_subbasins == [2]
    sampler = trainer.calibrate(upstream_only, "mc", repetitions=2, dbformat="ram")
    assert np.isfinite(trainer.get_best(sampler)["score"])


# ---- Per-subbasin structure variants and reach logging -----------------------------


def test_reach_values_are_logged_without_record_all(tmp_path):
    hydro_units = _split_units_with_reaches(86400.0)
    model = models.Socont(
        soil_storage_nb=2, surface_runoff="linear_storage", channel_routing="lag"
    )
    parameters = model.generate_parameters()
    parameters.set_values(
        {
            "A": 2000,
            "a_snow": 3,
            "k_slow_1": 0.9,
            "k_slow_2": 0.8,
            "k_quick": 1,
            "percol": 9.8,
            "channel_celerity": 1.0,
        }
    )
    out = tmp_path / "lag_light"
    out.mkdir()
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(out),
        start_date=_START,
        end_date="1981-03-31",
    )
    model.run(parameters=parameters, forcing=_forcing(hydro_units))
    model.dump_outputs(str(out))
    with hb.Results(str(out / "results.nc")) as results:
        labels = results.labels_aggregated
        assert "reach:outflow" in labels and "reach:storage" in labels
        outflow = results.get_subbasin_values("reach:outflow", 1)
        assert np.all(np.isfinite(outflow)) and outflow.sum() > 0
        # Without record_all, the stores are not recorded.
        assert not any(label.endswith(":water_content") for label in labels)
        assert list(results.get_subbasin_structure_ids()) == [1, 1]


def test_glacier_free_subbasin_carries_no_glacier_reservoir(tmp_path):
    """Subbasin 2 (upstream) holds the glacierized bands and gets the with-glacier
    variant with the shared glacier reservoirs; subbasin 1 (outlet) is glacier-free and
    builds the base variant, without them."""
    hu_csv = tmp_path / "hu.csv"
    hu_csv.write_text(
        "id,elevation,area_ground,area_glacier,subbasin\n"
        "-,m,km2,km2,-\n"
        "1,2000,2.0,0.0,1\n"
        "2,2500,1.0,1.0,2\n"
        "3,3000,2.0,0.0,1\n"
        "4,3500,0.5,1.5,2\n"
    )
    hydro_units = hb.HydroUnits(
        land_cover_types=["ground", "glacier"],
        land_cover_names=["ground", "glacier"],
    )
    hydro_units.load_from_csv(
        hu_csv,
        column_elevation="elevation",
        columns_areas={"ground": "area_ground", "glacier": "area_glacier"},
    )
    hydro_units.set_subbasins(pd.DataFrame({"id": [1, 2], "downstream": [0, 1]}))

    socont = models.Socont(
        surface_runoff="linear_storage",
        record_all=True,
        land_cover_names=["ground", "glacier"],
        land_cover_types=["ground", "glacier"],
    )
    parameters = socont.generate_parameters()
    parameters.set_values(
        {
            "a_snow": 3,
            "a_ice": 5,
            "A": 200,
            "k_slow": 0.001,
            "k_quick": 0.05,
            "k_snow": 0.1,
            "k_ice": 0.2,
        }
    )

    meteo = tmp_path / "meteo.csv"
    lines = ["date,precip(mm/day),temp(C),pet(mm/day)"]
    start = pd.Timestamp("2020-01-01")
    for i in range(40):
        day = start + pd.Timedelta(days=i)
        lines.append(f"{day.strftime('%d/%m/%Y')},5.0,3.0,1.0")
    meteo.write_text("\n".join(lines) + "\n")
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        meteo,
        column_time="date",
        time_format="%d/%m/%Y",
        content={
            "precipitation": "precip(mm/day)",
            "temperature": "temp(C)",
            "pet": "pet(mm/day)",
        },
    )
    forcing.spatialize_from_station_data(
        variable="temperature", ref_elevation=2500, gradient=-0.6
    )
    forcing.spatialize_from_station_data(variable="pet")
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=2500, gradient=0.0
    )

    out = tmp_path / "out"
    out.mkdir()
    socont.setup(
        spatial_structure=hydro_units,
        output_path=str(out),
        start_date="2020-01-01",
        end_date="2020-02-09",
    )
    socont.run(parameters=parameters, forcing=forcing)
    socont.dump_outputs(str(out))

    with hb.Results(str(out / "results.nc")) as results:
        assert list(results.subbasin_ids) == [2, 1]
        # The glacierized subbasin uses the with-glacier variant (2), the other the base
        assert list(results.get_subbasin_structure_ids()) == [2, 1]
        ice = "glacier_area_icemelt_storage:water_content"
        assert np.all(np.isnan(results.get_subbasin_values(ice, 1)))
        assert np.all(np.isfinite(results.get_subbasin_values(ice, 2)))
        assert np.all(np.isfinite(results.get_subbasin_values("outlet", 1)))
    assert socont.get_total_outlet_discharge() > 0
    assert np.isfinite(socont.get_total_water_storage_changes())


# ---- Muskingum-Cunge, variable celerity, local runoff, discharge in m3/s -------------


def _split_units_with_geometry(length_m: float = 86400.0) -> hb.HydroUnits:
    """The split Sitter with the full reach geometry the physical schemes need."""
    hydro_units = _load_units(STGALLEN_HUS)
    ids = hydro_units.hydro_units["id"].iloc[:, 0].to_numpy()
    hydro_units.add_property(("subbasin", "-"), np.where(ids >= _UPSTREAM_MIN_ID, 2, 1))
    hydro_units.set_subbasins(
        pd.DataFrame(
            {
                "id": [1, 2],
                "downstream": [0, 1],
                "length": [length_m, 3000.0],
                "slope": [0.004, 0.02],
                "width": [15.0, 6.0],
                "manning": [0.035, 0.05],
            }
        )
    )
    return hydro_units


def _run_scheme(out: Path, hydro_units, scheme: str, **params) -> models.Socont:
    model = models.Socont(
        soil_storage_nb=2,
        surface_runoff="linear_storage",
        channel_routing=scheme,
        route_local_runoff=params.pop("route_local_runoff", False),
    )
    parameters = model.generate_parameters()
    parameters.set_values(
        {
            "A": 2000,
            "a_snow": 3,
            "k_slow_1": 0.9,
            "k_slow_2": 0.8,
            "k_quick": 1,
            "percol": 9.8,
            **params,
        }
    )
    out.mkdir(parents=True, exist_ok=True)
    model.setup(
        spatial_structure=hydro_units,
        output_path=str(out),
        start_date=_START,
        end_date="1981-06-30",
    )
    model.run(parameters=parameters, forcing=_forcing(hydro_units))
    return model


def test_muskingum_cunge_parameters_replace_the_celerity():
    """The geometry drives the scheme, so there is no celerity to calibrate."""
    params = models.Socont(
        surface_runoff="linear_storage", channel_routing="muskingum_cunge"
    ).generate_parameters()
    assert params.has("channel_width")
    assert params.has("channel_manning")
    assert not params.has("channel_celerity")
    assert not params.has("muskingum_x")
    # The geometry has defaults, so nothing has to be set for the model to run.
    assert params.get("channel_width") == 10.0
    assert params.get("channel_manning") == 0.035
    undefined = params.get_undefined()
    assert "width" not in undefined and "manning" not in undefined

    # The lag and Muskingum schemes keep the celerity, plus its optional variation.
    lag = models.Socont(
        surface_runoff="linear_storage", channel_routing="lag"
    ).generate_parameters()
    assert lag.has("channel_celerity")
    assert lag.has("channel_celerity_exponent")
    assert lag.has("channel_reference_discharge")


def test_muskingum_cunge_needs_the_reach_slope(tmp_path):
    hydro_units = _split_units_with_reaches(86400.0)  # length but no slope
    model = models.Socont(
        soil_storage_nb=2,
        surface_runoff="linear_storage",
        channel_routing="muskingum_cunge",
    )
    with pytest.raises(hb.ConfigurationError, match="slope"):
        model.setup(
            spatial_structure=hydro_units,
            output_path=str(tmp_path),
            start_date=_START,
            end_date="1981-03-31",
        )


def test_muskingum_cunge_runs_and_conserves_mass(tmp_path):
    hydro_units = _split_units_with_geometry()
    model = _run_scheme(tmp_path / "mc", hydro_units, "muskingum_cunge")
    model.dump_outputs(str(tmp_path / "mc"))

    outlet = model.get_outlet_discharge()
    assert np.all(outlet >= 0)
    assert np.all(np.isfinite(outlet))
    assert model.get_total_outlet_discharge() > 0

    with hb.Results(str(tmp_path / "mc" / "results.nc")) as results:
        storage = results.get_subbasin_values("reach:storage", 1)
        assert np.all(storage >= 0)
        # The reach holds water in transit at least once.
        assert storage.max() > 0

    # The same catchment routed instantaneously carries the same total, up to what is
    # still in the reach at the end.
    none = _run_scheme(tmp_path / "none", _split_units_with_geometry(), "none")
    assert model.get_total_outlet_discharge() < none.get_total_outlet_discharge()
    assert model.get_total_outlet_discharge() == pytest.approx(
        none.get_total_outlet_discharge(), rel=0.02
    )


def test_variable_celerity_speeds_up_the_routing(tmp_path):
    """A positive exponent makes the wave faster than the reference celerity as soon as
    the discharge exceeds the reference."""
    slow = _run_scheme(
        tmp_path / "const", _split_units_with_geometry(), "lag", channel_celerity=0.5
    )
    fast = _run_scheme(
        tmp_path / "var",
        _split_units_with_geometry(),
        "lag",
        channel_celerity=0.5,
        channel_celerity_exponent=0.4,
        channel_reference_discharge=0.5,
    )
    # Same water, delivered earlier: more of it has left the reach by the end.
    assert fast.get_total_outlet_discharge() > slow.get_total_outlet_discharge()


def test_local_runoff_can_travel_through_the_reach(tmp_path):
    direct = _run_scheme(
        tmp_path / "direct", _split_units_with_geometry(), "lag", channel_celerity=0.5
    )
    routed = _run_scheme(
        tmp_path / "routed",
        _split_units_with_geometry(),
        "lag",
        channel_celerity=0.5,
        route_local_runoff=True,
    )
    assert direct.route_local_runoff is False
    assert routed.route_local_runoff is True
    # The local runoff is delayed too, so less water has reached the outlet by the end
    # and the peak is not earlier than before.
    assert routed.get_total_outlet_discharge() < direct.get_total_outlet_discharge()
    assert np.argmax(routed.get_outlet_discharge()) >= np.argmax(
        direct.get_outlet_discharge()
    )
    assert np.all(routed.get_outlet_discharge() >= 0)


def test_discharge_available_in_cubic_meters_per_second(runs):
    network = runs["network"]
    areas = network.get_subbasin_areas()
    outlet_mm = network.get_outlet_discharge()
    outlet_m3s = network.get_outlet_discharge(units="m3/s")
    expected = outlet_mm * areas.loc[1, "drained"] / 1000.0 / 86400.0
    np.testing.assert_allclose(outlet_m3s, expected, rtol=1e-12)

    upstream_m3s = network.get_subbasin_discharge(2, units="m3/s")
    np.testing.assert_allclose(
        upstream_m3s,
        network.get_subbasin_discharge(2) * areas.loc[2, "drained"] / 1000.0 / 86400.0,
        rtol=1e-12,
    )
    # The upstream subbasin carries less water than the whole catchment.
    assert upstream_m3s.sum() < outlet_m3s.sum()
    # The default is unchanged.
    np.testing.assert_allclose(network.get_outlet_discharge("mm"), outlet_mm, rtol=0)
    with pytest.raises(hb.ConfigurationError):
        network.get_outlet_discharge(units="gallons")

    # The same conversion from the results file.
    with hb.Results(str(runs["results"])) as results:
        np.testing.assert_allclose(results.get_discharge(), expected, rtol=1e-9)
        np.testing.assert_allclose(results.get_discharge(2), upstream_m3s, rtol=1e-9)
        np.testing.assert_allclose(
            results.get_discharge(units="mm"), outlet_mm, rtol=1e-9
        )
        with pytest.raises(hb.DataError):
            results.get_discharge(units="furlongs")


# ---- Network diagnostics ------------------------------------------------------------


def test_network_graph_describes_the_subbasin_tree(tmp_path):
    """The graph reports what the routing uses: the tree, the reaches, the times."""
    hydro_units = _split_units_with_geometry()
    model = _run_scheme(
        tmp_path / "graph",
        hydro_units,
        "muskingum",
        channel_celerity=1,
        muskingum_x=0.2,
    )
    graph = model.get_network_graph()

    assert graph.scheme == "muskingum"
    assert [s.id for s in graph.subbasins] == [2, 1]  # processing order, outlet last
    upstream = graph.get_subbasin(2)
    downstream = graph.get_subbasin(1)
    assert upstream.downstream == 1
    assert downstream.downstream == 0
    assert downstream.length == pytest.approx(86400.0)
    assert upstream.length == pytest.approx(3000.0)
    assert downstream.slope == pytest.approx(0.004)

    # K = length / celerity: 86400 m at 1 m/s is exactly one day.
    assert downstream.travel_time == pytest.approx(1.0)
    assert upstream.travel_time == pytest.approx(3000.0 / 86400.0)

    # A subbasin's own reach routes what comes from upstream, so the water it sends
    # down crosses the reaches *below* it: one day, here.
    times = graph.get_time_to_outlet()
    assert times[1] == pytest.approx(0.0)
    assert times[2] == pytest.approx(1.0)

    assert [s.id for s in graph.get_headwaters()] == [2]
    path, slowest = graph.get_longest_path()
    assert [s.id for s in path] == [2, 1]
    assert slowest == pytest.approx(1.0)

    # The areas are the ones the model computes, and the table carries them.
    table = graph.to_dataframe()
    assert list(table.index) == [2, 1]
    assert table.at[1, "area_drained"] > table.at[2, "area_drained"]
    assert table.at[1, "time_to_outlet"] == pytest.approx(0.0)
    assert graph.to_dict()["channel_routing"] == "muskingum"


def test_network_summary_reports_the_reaches(tmp_path):
    hydro_units = _split_units_with_geometry()
    model = _run_scheme(
        tmp_path / "text", hydro_units, "muskingum", channel_celerity=1, muskingum_x=0.2
    )
    text = model.network_summary()

    assert "river network" in text
    assert "muskingum" in text
    assert "subbasin 1" in text and "subbasin 2" in text
    assert "Headwaters: 1" in text
    assert "Slowest path" in text


def test_network_graph_reports_the_muskingum_cunge_sub_reaches(tmp_path):
    """The criterion divides the long reach, and the report says so."""
    hydro_units = _split_units_with_geometry()
    model = _run_scheme(tmp_path / "cunge", hydro_units, "muskingum_cunge")
    graph = model.get_network_graph()

    assert graph.get_subbasin(1).subreaches > 1
    assert graph.get_subbasin(2).subreaches == 1
    assert graph.get_subbasin(1).travel_time > 0


def test_network_graph_needs_a_model_that_is_set_up():
    model = models.Socont(surface_runoff="linear_storage", channel_routing="lag")
    with pytest.raises(hb.ModelError):
        model.get_network_graph()


def test_reach_water_balance_closes(tmp_path):
    """Nothing is lost in a reach: what entered it left it or is still in transit."""
    hydro_units = _split_units_with_geometry()
    out = tmp_path / "balance"
    model = _run_scheme(
        out, hydro_units, "muskingum", channel_celerity=1, muskingum_x=0.2
    )
    model.dump_outputs(str(out))

    with hb.Results(str(out / "results.nc")) as results:
        balance = results.get_reach_water_balance()

    assert list(balance.index) == [2, 1]
    # The downstream reach receives the upstream subbasin, the headwater reach nothing.
    assert balance.at[1, "inflow"] > 0
    assert balance.at[2, "inflow"] == pytest.approx(0.0)
    # A one-day travel time leaves water in transit at the end of the run.
    assert balance.at[1, "storage"] > 0
    for subbasin_id in (1, 2):
        assert balance.at[subbasin_id, "balance"] == pytest.approx(0.0, abs=1.0)
        assert abs(balance.at[subbasin_id, "relative"]) < 1e-6


def test_reach_water_balance_needs_the_reach_values(tmp_path):
    hydro_units = _split_units_with_geometry()
    out = tmp_path / "no_routing"
    model = _run_scheme(out, hydro_units, "none")
    model.dump_outputs(str(out))

    with hb.Results(str(out / "results.nc")) as results:
        with pytest.raises(hb.DataError):
            results.get_reach_water_balance()
