from pathlib import Path

import pytest

import hydrobricks as hb
import hydrobricks.models as models

_TEST_DIR = Path(__file__).resolve().parent
SITTER_HUS = (
    _TEST_DIR
    / ".."
    / ".."
    / "tests"
    / "files"
    / "catchments"
    / "ch_sitter_appenzell"
    / "hydro_units_elevation.csv"
)
SITTER_METEO = (
    _TEST_DIR
    / ".."
    / ".."
    / "tests"
    / "files"
    / "catchments"
    / "ch_sitter_appenzell"
    / "meteo.csv"
)

SUBLIMATION_KINDS_AND_PARAMS = [
    ("sublimation:constant", ["sublimation_rate"]),
    ("sublimation:pet", ["sublimation_pet_factor"]),
]


@pytest.mark.parametrize("kind,expected_params", SUBLIMATION_KINDS_AND_PARAMS)
def test_sublimation_registers_parameters(kind, expected_params):
    parameter_set = hb.ParameterSet()
    parameter_set._generate_process_parameters(
        "ground_snowpack", {"processes": {"sublimation": {"kind": kind}}}
    )
    names = set(parameter_set.parameters["name"])
    for param in expected_params:
        assert param in names


@pytest.mark.parametrize("kind,_", SUBLIMATION_KINDS_AND_PARAMS)
def test_socont_creation_with_sublimation(kind, _):
    # The model must build (structure generation and parameter registration) with a
    # sublimation process on the snowpacks.
    socont = models.Socont(snow_sublimation_process=kind)
    parameters = socont.generate_parameters()
    names = set(parameters.parameters["name"])
    expected = dict(SUBLIMATION_KINDS_AND_PARAMS)[kind]
    for param in expected:
        assert param in names


def test_sublimation_closes_water_balance_and_removes_snow(tmp_path):
    def build_and_run(sublimation_process):
        socont = models.Socont(
            soil_storage_nb=1,
            surface_runoff="linear_storage",
            snow_sublimation_process=sublimation_process,
            record_all=True,
        )
        parameters = socont.generate_parameters()
        parameters.set_values({"a_snow": 3, "k_quick": 0.05, "A": 200, "k_slow": 0.001})
        if sublimation_process == "sublimation:constant":
            parameters.set_values({"sublimation_rate": 0.5})
        parameters.add_data_parameter("precip_correction_factor", 1)
        parameters.add_data_parameter("precip_gradient", 0.05)
        parameters.add_data_parameter("temp_gradients", -0.6)

        hydro_units = hb.HydroUnits()
        hydro_units.load_from_csv(
            SITTER_HUS, column_elevation="elevation", column_area="area"
        )

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

        # The run must span the full forcing period: get_total_precipitation sums
        # the whole loaded series, so a sub-period would not close the balance.
        socont.setup(
            spatial_structure=hydro_units,
            output_path=str(tmp_path),
            start_date="1981-01-01",
            end_date="2020-12-31",
        )
        socont.run(parameters=parameters, forcing=forcing)

        precip_total = forcing.get_total_precipitation()
        discharge_total = socont.get_total_outlet_discharge()
        et_total = socont.get_total_et()
        storage_change = socont.get_total_water_storage_changes()
        snow_change = socont.get_total_snow_storage_changes()
        balance = (
            discharge_total + et_total + storage_change + snow_change - precip_total
        )
        return balance, et_total

    balance_sub, et_with = build_and_run("sublimation:constant")
    # The sublimated snow is booked as an atmosphere (ET) term, so the balance
    # (discharge + et + storage + snow - precip) must still close.
    assert balance_sub == pytest.approx(0, abs=1e-6)

    # Sublimation should add an atmospheric loss compared with a run without it.
    balance_none, et_without = build_and_run(None)
    assert balance_none == pytest.approx(0, abs=1e-6)
    assert et_with > et_without
