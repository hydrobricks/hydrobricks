import datetime
import os.path
import tempfile
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
SITTER_HUS = TEST_FILES_DIR / "ch_sitter_appenzell" / "hydro_units_elevation.csv"
SITTER_METEO = TEST_FILES_DIR / "ch_sitter_appenzell" / "meteo.csv"
SITTER_DISCHARGE = TEST_FILES_DIR / "ch_sitter_appenzell" / "discharge.csv"


def test_socont_creation():
    model = models.Socont()
    assert model.name == "socont"


def test_setup_land_cover_mismatch_raises(tmp_path):
    """A model whose land cover disagrees with the hydro units fails at setup()
    with a clear ConfigurationError rather than a confusing downstream error."""
    hydro_units = hb.HydroUnits()  # defaults to a single 'open' cover
    hydro_units.load_from_csv(
        SITTER_HUS, column_elevation="elevation", column_area="area"
    )

    socont = models.Socont(
        land_cover_names=["open", "glacier"],
        land_cover_types=["open", "glacier"],
    )
    out = tmp_path / "out"
    out.mkdir()
    with pytest.raises(hb.ConfigurationError):
        socont.setup(
            spatial_structure=hydro_units,
            output_path=str(out),
            start_date="2020-01-01",
            end_date="2020-02-09",
        )


def test_setup_land_cover_match_ok():
    """Matching land cover (the common single-'open' case) passes the check; the
    'ground'/'open' generic aliases are treated as equivalent."""
    hydro_units = hb.HydroUnits()  # single 'open' cover
    models.Socont()._check_land_cover_matches_structure(hydro_units)

    hu_ground = hb.HydroUnits(
        land_cover_types=["ground", "glacier"],
        land_cover_names=["ground", "glacier"],
    )
    socont = models.Socont(
        land_cover_names=["ground", "glacier"],
        land_cover_types=["ground", "glacier"],
    )
    socont._check_land_cover_matches_structure(hu_ground)


def test_socont_glacier_brick_absent_where_no_glacier(tmp_path):
    """Units with no glacier use a glacier-free structure variant and therefore
    carry no glacier brick at all (logged as NaN), while glacier units do."""
    # Mixed catchment: bands 1 and 3 have no glacier; bands 2 and 4 do.
    hu_csv = tmp_path / "hu.csv"
    hu_csv.write_text(
        "id,elevation,area_ground,area_glacier\n"
        "-,m,km2,km2\n"
        "1,2000,2.0,0.0\n"
        "2,2500,1.0,1.0\n"
        "3,3000,2.0,0.0\n"
        "4,3500,0.5,1.5\n"
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
    parameters.add_data_parameter("precip_correction_factor", 1)
    parameters.add_data_parameter("precip_gradient", 0.05)
    parameters.add_data_parameter("temp_gradients", -0.6)

    # Synthetic uniform forcing over a short period.
    meteo = tmp_path / "meteo.csv"
    lines = ["date,precip(mm/day),temp(C),pet(mm/day)"]
    start = datetime.date(2020, 1, 1)
    for i in range(40):
        d = start + datetime.timedelta(days=i)
        lines.append(f"{d.strftime('%d/%m/%Y')},5.0,3.0,1.0")
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

    results = hb.Results(str(out / "results.nc"))
    structure_ids = results.get_hydro_units_structure_ids()
    glacier_areas = results.get_land_cover_areas("glacier")  # (units, time)
    glacier_state = results.get_hydro_units_values(
        "glacier:water_content"
    )  # (units, time)
    n_units = len(results.hydro_units_ids)

    # Two structure variants are in use (with-glacier and glacier-free).
    assert len(set(structure_ids.tolist())) == 2

    # The glacier brick exists exactly where glacier is present; elsewhere it is
    # omitted (NaN), not a zero-area brick.
    for i in range(n_units):
        has_glacier = not bool(np.all(np.isnan(glacier_areas[i, :])))
        brick_present = not bool(np.all(np.isnan(glacier_state[i, :])))
        assert has_glacier == brick_present


def test_get_results_and_dump_outputs_return_path(tmp_path):
    """dump_outputs() returns the written file path and get_results() returns a ready
    Results reader, so users no longer hardcode the 'results.nc' filename."""
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        SITTER_HUS, column_elevation="elevation", column_area="area"
    )

    meteo = tmp_path / "meteo.csv"
    lines = ["date,precip(mm/day),temp(C),pet(mm/day)"]
    start = datetime.date(2020, 1, 1)
    for i in range(40):
        d = start + datetime.timedelta(days=i)
        lines.append(f"{d.strftime('%d/%m/%Y')},5.0,3.0,1.0")
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
        variable="temperature", ref_elevation=1250, gradient=-0.6
    )
    forcing.spatialize_from_station_data(variable="pet")
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=1250, gradient=0.0
    )

    socont = models.Socont(surface_runoff="linear_storage", record_all=True)
    parameters = socont.generate_parameters()
    parameters.set_values({"a_snow": 3, "A": 200, "k_slow_1": 0.01, "k_quick": 0.05})

    out = tmp_path / "out"
    out.mkdir()
    socont.setup(
        spatial_structure=hydro_units,
        output_path=str(out),
        start_date="2020-01-01",
        end_date="2020-02-09",
    )
    socont.run(parameters=parameters, forcing=forcing)

    # dump_outputs returns the full path (and defaults to the setup output path).
    dumped = socont.dump_outputs()
    assert dumped == os.path.join(str(out), "results.nc")
    assert os.path.isfile(dumped)

    # get_results dumps and hands back a reader without the filename dance.
    results = socont.get_results()
    assert isinstance(results, hb.Results)
    assert len(results.hydro_units_ids) == len(hydro_units.hydro_units)


def test_socont_creation_with_soil_storage_nb():
    models.Socont(soil_storage_nb=2)


def test_socont_creation_with_solver():
    models.Socont(solver="runge_kutta")


def test_socont_creation_with_surface_runoff():
    models.Socont(surface_runoff="linear_storage")


def test_socont_creation_with_land_covers():
    cover_names = ["ground", "aletsch_glacier"]
    cover_types = ["ground", "glacier"]
    models.Socont(land_cover_names=cover_names, land_cover_types=cover_types)


def test_create_json_config_file_created():
    model = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage")
    with tempfile.TemporaryDirectory() as tmp_dir:
        model.create_config_file(tmp_dir, "structure", "json")
        assert os.path.isfile(Path(tmp_dir) / "structure.json")


def test_create_json_config_file_content():
    cover_names = ["ground", "aletsch_glacier"]
    cover_types = ["ground", "glacier"]
    model = models.Socont(
        solver="runge_kutta",
        soil_storage_nb=2,
        land_cover_names=cover_names,
        land_cover_types=cover_types,
        surface_runoff="linear_storage",
    )
    with tempfile.TemporaryDirectory() as tmp_dir:
        model.create_config_file(tmp_dir, "structure", "json")
        txt = Path(tmp_dir + "/structure.json").read_text()
        assert '"base": "socont"' in txt
        assert '"solver": "runge_kutta"' in txt
        assert '"soil_storage_nb": 2' in txt
        assert '"surface_runoff": "linear_storage"' in txt
        assert "ground" in txt
        assert "aletsch_glacier" in txt


def test_create_yaml_config_file_created():
    model = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage")
    with tempfile.TemporaryDirectory() as tmp_dir:
        model.create_config_file(tmp_dir, "structure", "yaml")
        assert os.path.isfile(Path(tmp_dir) / "structure.yaml")


def test_create_yaml_config_file_content():
    cover_names = ["ground", "aletsch_glacier"]
    cover_types = ["ground", "glacier"]
    model = models.Socont(
        solver="runge_kutta",
        soil_storage_nb=2,
        land_cover_names=cover_names,
        land_cover_types=cover_types,
        surface_runoff="linear_storage",
    )
    with tempfile.TemporaryDirectory() as tmp_dir:
        model.create_config_file(tmp_dir, "structure", "yaml")
        txt = Path(tmp_dir + "/structure.yaml").read_text()
        assert "base: socont" in txt
        assert "solver: runge_kutta" in txt
        assert "soil_storage_nb: 2" in txt
        assert "surface_runoff: linear_storage" in txt
        assert "ground" in txt
        assert "aletsch_glacier" in txt


def test_socont_with_1_soil_storage_closes_water_balance():
    tmp_dir = tempfile.TemporaryDirectory()

    # Model options
    socont = models.Socont(
        soil_storage_nb=1, surface_runoff="linear_storage", record_all=True
    )

    # Parameters
    parameters = socont.generate_parameters()
    parameters.set_values({"a_snow": 3, "k_quick": 0.05, "A": 200, "k_slow": 0.001})
    parameters.add_data_parameter("precip_correction_factor", 1)
    parameters.add_data_parameter("precip_gradient", 0.05)
    parameters.add_data_parameter("temp_gradients", -0.6)

    # Preparation of the hydro units
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        SITTER_HUS, column_elevation="elevation", column_area="area"
    )

    # Preparation of the forcing data
    station_temp_alt = 1250  # Temperature reference altitude
    station_precip_alt = 1250  # Precipitation reference altitude
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        SITTER_METEO,
        column_time="date",
        time_format="%d/%m/%Y",
        content={
            "precipitation": "precip(mm/day)",
            "temperature": "temp(C)",
            "pet": "pet_sim(mm/day)",
        },
    )
    forcing.spatialize_from_station_data(
        variable="temperature",
        ref_elevation=station_temp_alt,
        gradient=parameters.get("temp_gradients"),
    )
    forcing.spatialize_from_station_data(variable="pet")
    forcing.correct_station_data(
        variable="precipitation",
        correction_factor=parameters.get("precip_correction_factor"),
    )
    forcing.spatialize_from_station_data(
        variable="precipitation",
        ref_elevation=station_precip_alt,
        gradient=parameters.get("precip_gradient"),
    )

    socont.setup(
        spatial_structure=hydro_units,
        output_path=tmp_dir.name,
        start_date="1981-01-01",
        end_date="2020-12-31",
    )
    socont.run(parameters=parameters, forcing=forcing)

    # Water balance
    precip_total = forcing.get_total_precipitation()
    discharge_total = socont.get_total_outlet_discharge()
    et_total = socont.get_total_et()
    storage_change = socont.get_total_water_storage_changes()
    snow_change = socont.get_total_snow_storage_changes()
    balance = discharge_total + et_total + storage_change + snow_change - precip_total

    assert balance == pytest.approx(0, abs=1e-8)

    try:
        tmp_dir.cleanup()
    except Exception:
        print("Could not remove temporary directory.")


def test_socont_with_2_soil_storages_closes_water_balance():
    tmp_dir = tempfile.TemporaryDirectory()

    # Model options
    socont = models.Socont(
        soil_storage_nb=2, surface_runoff="linear_storage", record_all=True
    )

    # Parameters
    parameters = socont.generate_parameters()
    parameters.set_values(
        {
            "a_snow": 3,
            "k_quick": 0.05,
            "A": 200,
            "k_slow_1": 0.001,
            "percol": 0.5,
            "k_slow_2": 0.005,
        }
    )
    parameters.add_data_parameter("precip_correction_factor", 1)
    parameters.add_data_parameter("precip_gradient", 0.05)
    parameters.add_data_parameter("temp_gradients", -0.6)

    # Preparation of the hydro units
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        SITTER_HUS, column_elevation="elevation", column_area="area"
    )

    # Preparation of the forcing data
    station_temp_alt = 1250  # Temperature reference altitude
    station_precip_alt = 1250  # Precipitation reference altitude
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        SITTER_METEO,
        column_time="date",
        time_format="%d/%m/%Y",
        content={
            "precipitation": "precip(mm/day)",
            "temperature": "temp(C)",
            "pet": "pet_sim(mm/day)",
        },
    )
    forcing.spatialize_from_station_data(
        variable="temperature",
        ref_elevation=station_temp_alt,
        gradient=parameters.get("temp_gradients"),
    )
    forcing.spatialize_from_station_data(variable="pet")
    forcing.correct_station_data(
        variable="precipitation",
        correction_factor=parameters.get("precip_correction_factor"),
    )
    forcing.spatialize_from_station_data(
        variable="precipitation",
        ref_elevation=station_precip_alt,
        gradient=parameters.get("precip_gradient"),
    )

    socont.setup(
        spatial_structure=hydro_units,
        output_path=tmp_dir.name,
        start_date="1981-01-01",
        end_date="2020-12-31",
    )
    socont.run(parameters=parameters, forcing=forcing)

    # Water balance
    precip_total = forcing.get_total_precipitation()
    discharge_total = socont.get_total_outlet_discharge()
    et_total = socont.get_total_et()
    storage_change = socont.get_total_water_storage_changes()
    snow_change = socont.get_total_snow_storage_changes()
    balance = discharge_total + et_total + storage_change + snow_change - precip_total

    assert balance == pytest.approx(0, abs=1e-8)

    try:
        tmp_dir.cleanup()
    except Exception:
        print("Could not remove temporary directory.")


def test_error_is_raised_if_parameter_missing():
    tmp_dir = tempfile.TemporaryDirectory()

    # Model options
    socont = models.Socont(
        soil_storage_nb=2, surface_runoff="linear_storage", record_all=True
    )

    # Parameters
    parameters = socont.generate_parameters()
    parameters.set_values({"a_snow": 3, "k_quick": 0.05, "A": 200, "k_slow_1": 0.001})

    # Preparation of the hydro units
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        SITTER_HUS, column_elevation="elevation", column_area="area"
    )

    socont.setup(
        spatial_structure=hydro_units,
        output_path=tmp_dir.name,
        start_date="1981-01-01",
        end_date="2020-12-31",
    )

    with pytest.raises(
        hb.ConfigurationError, match=r"Some parameters were not defined*"
    ):
        socont.run(parameters=parameters)

    try:
        tmp_dir.cleanup()
    except Exception:
        print("Could not remove temporary directory.")


@pytest.mark.parametrize(
    "kind",
    [
        "transport:snow_redistribution_frey",
        "transport:snow_redistribution_frey_dynamic",
    ],
)
def test_snow_redistribution_frey_registers_parameters(kind):
    # The snow redistribution parameters must be registered (regression: an early
    # return in the snow parameter generation used to skip them entirely).
    socont = models.Socont(snow_redistribution=kind)
    parameters = socont.generate_parameters()
    names = set(parameters.parameters["name"])
    assert "snow_holding_capacity" in names
    assert "correction" in names
    assert "rho_max" in names

    # The holding capacity is registered per land cover snowpack (not globally).
    hv_rows = parameters.parameters[
        parameters.parameters["name"] == "snow_holding_capacity"
    ]
    assert all("snowpack" in c for c in hv_rows["component"])


@pytest.mark.parametrize(
    "kind",
    [
        "transport:snow_redistribution_frey",
        "transport:snow_redistribution_frey_dynamic",
    ],
)
def test_snow_redistribution_frey_closes_water_balance(kind):
    tmp_dir = tempfile.TemporaryDirectory()

    socont = models.Socont(
        soil_storage_nb=1,
        surface_runoff="linear_storage",
        snow_redistribution=kind,
        record_all=True,
    )

    parameters = socont.generate_parameters()
    parameters.set_values({"a_snow": 3, "k_quick": 0.05, "A": 200, "k_slow": 0.001})
    parameters.add_data_parameter("precip_correction_factor", 1)
    parameters.add_data_parameter("precip_gradient", 0.05)
    parameters.add_data_parameter("temp_gradients", -0.6)

    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        SITTER_HUS, column_elevation="elevation", column_area="area"
    )

    # A slope property is required by the redistribution process.
    n = len(hydro_units.get_ids())
    hydro_units.add_property(("slope", "degrees"), np.full(n, 30.0))
    hydro_units.populate_bounded_instance()

    # A downslope connectivity chain (each band drains to the next lower one) so the
    # redistribution is actually active. Added after populate_bounded_instance, which
    # clears the settings.
    df = hydro_units.hydro_units
    order = sorted(
        range(n), key=lambda i: float(df[("elevation", "m")].iloc[i]), reverse=True
    )
    ids = [int(df[("id", "-")].iloc[i]) for i in order]
    for upper, lower in zip(ids[:-1], ids[1:]):
        hydro_units.settings.add_lateral_connection(upper, lower, 1.0)

    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        SITTER_METEO,
        column_time="date",
        time_format="%d/%m/%Y",
        content={
            "precipitation": "precip(mm/day)",
            "temperature": "temp(C)",
            "pet": "pet_sim(mm/day)",
        },
    )
    forcing.spatialize_from_station_data(
        variable="temperature",
        ref_elevation=1250,
        gradient=parameters.get("temp_gradients"),
    )
    forcing.spatialize_from_station_data(variable="pet")
    forcing.correct_station_data(
        variable="precipitation",
        correction_factor=parameters.get("precip_correction_factor"),
    )
    forcing.spatialize_from_station_data(
        variable="precipitation",
        ref_elevation=1250,
        gradient=parameters.get("precip_gradient"),
    )

    socont.setup(
        spatial_structure=hydro_units,
        output_path=tmp_dir.name,
        start_date="1981-01-01",
        end_date="2020-12-31",
    )
    socont.run(parameters=parameters, forcing=forcing)

    # Water balance must close: redistribution moves snow between units without
    # creating or destroying mass.
    precip_total = forcing.get_total_precipitation()
    discharge_total = socont.get_total_outlet_discharge()
    et_total = socont.get_total_et()
    storage_change = socont.get_total_water_storage_changes()
    snow_change = socont.get_total_snow_storage_changes()
    balance = discharge_total + et_total + storage_change + snow_change - precip_total

    assert balance == pytest.approx(0, abs=1e-6)

    try:
        tmp_dir.cleanup()
    except Exception:
        print("Could not remove temporary directory.")


# ---------------------------------------------------------------------------
# Periods and spin-up
# ---------------------------------------------------------------------------
def _build_simple_socont_run(tmp_path, subdir, **setup_kwargs):
    """Build, set up and run a small SOCONT on synthetic constant forcing.

    The synthetic meteo spans 2020-01-01..2020-04-29 (120 days); the model period
    comes from ``setup_kwargs`` and may be shorter (the forcing may exceed the
    modelling period).
    """
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        SITTER_HUS, column_elevation="elevation", column_area="area"
    )

    meteo = tmp_path / f"meteo_{subdir}.csv"
    lines = ["date,precip(mm/day),temp(C),pet(mm/day)"]
    start = datetime.date(2020, 1, 1)
    for i in range(120):
        d = start + datetime.timedelta(days=i)
        lines.append(f"{d.strftime('%d/%m/%Y')},5.0,3.0,1.0")
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
        variable="temperature", ref_elevation=1250, gradient=-0.6
    )
    forcing.spatialize_from_station_data(variable="pet")
    forcing.spatialize_from_station_data(
        variable="precipitation", ref_elevation=1250, gradient=0.0
    )

    socont = models.Socont(surface_runoff="linear_storage")
    parameters = socont.generate_parameters()
    parameters.set_values({"a_snow": 3, "A": 200, "k_slow_1": 0.01, "k_quick": 0.05})

    out = tmp_path / subdir
    out.mkdir()
    socont.setup(spatial_structure=hydro_units, output_path=str(out), **setup_kwargs)
    socont.run(parameters=parameters, forcing=forcing)
    return socont


def test_setup_with_period_object(tmp_path):
    """setup(period=...) is equivalent to start/end dates, and the forcing may be
    longer than the modelling period (only coverage is required)."""
    period = hb.Period("2020-01-10", "2020-02-28")
    socont = _build_simple_socont_run(tmp_path, "period", period=period)

    assert socont.start_date == "2020-01-10"
    assert socont.end_date == "2020-02-28"
    assert socont.period == period
    time = socont.get_recorded_time()
    assert len(time) == period.n_days
    assert len(socont.get_outlet_discharge()) == period.n_days


def test_setup_period_and_dates_are_exclusive(tmp_path):
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        SITTER_HUS, column_elevation="elevation", column_area="area"
    )
    socont = models.Socont()
    with pytest.raises(hb.ConfigurationError):
        socont.setup(
            spatial_structure=hydro_units,
            output_path=str(tmp_path),
            start_date="2020-01-01",
            period=("2020-01-01", "2020-02-09"),
        )
    with pytest.raises(hb.ConfigurationError):
        models.Socont().setup(spatial_structure=hydro_units, output_path=str(tmp_path))


def test_spinup_warms_the_initial_state(tmp_path):
    """With a spin-up, the run starts from warmed-up storages: under constant
    forcing the first-day discharge exceeds the cold start's."""
    kwargs = dict(start_date="2020-01-01", end_date="2020-02-29")
    cold = _build_simple_socont_run(tmp_path, "cold", **kwargs)
    warm = _build_simple_socont_run(tmp_path, "warm", spinup=30, **kwargs)

    assert warm.spinup_days == 30
    cold_sim = cold.get_outlet_discharge()
    warm_sim = warm.get_outlet_discharge()
    assert len(cold_sim) == len(warm_sim)
    assert warm_sim[0] > cold_sim[0]


def test_eval_with_period(tmp_path):
    """eval(period=...) scores a date slice of the simulation."""
    socont = _build_simple_socont_run(
        tmp_path, "eval", start_date="2020-01-01", end_date="2020-02-29"
    )
    sim = socont.get_outlet_discharge()
    time = socont.get_recorded_time()

    # Perfect observations on a sub-period: NSE == 1 on that slice.
    period = hb.Period("2020-02-01", "2020-02-20")
    nse = socont.eval("nse", sim.copy(), period=period)
    assert nse == pytest.approx(1.0)

    # DischargeObservations are sliced by their own dates.
    obs = hb.DischargeObservations("2020-01-01", "2020-02-29")
    obs.time = pd.Series(time)
    obs.data = [sim.copy()]
    assert socont.eval("nse", obs, period=period) == pytest.approx(1.0)

    # warmup and period are mutually exclusive; uncovered periods raise.
    with pytest.raises(hb.ConfigurationError):
        socont.eval("nse", sim, warmup=10, period=period)
    with pytest.raises(hb.ConfigurationError):
        socont.eval("nse", sim, period=("2019-01-01", "2020-02-20"))
