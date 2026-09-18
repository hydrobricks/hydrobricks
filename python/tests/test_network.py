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
