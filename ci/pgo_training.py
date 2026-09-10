"""Training workload for the profile-guided build.

Run this against an instrumented ``_hydrobricks`` module to collect the branch
profile the optimized build feeds on. The point of the workload is coverage of
the paths a real simulation takes, not speed: it runs the model structures over
a multi-year daily period, with the solvers users are likely to select. Keep it
representative rather than exhaustive -- a profile dominated by code nobody runs
is worse than no profile.

Sequence with MSVC (``USE_PGO`` requires ``USE_LTO``, which is on by default)::

    cmake -S . -B build -DUSE_PGO=generate
    cmake --build build --target _hydrobricks --config Release
    copy build\\core\\bindings\\_hydrobricks.*.pyd python\\src\\hydrobricks\\
    python ci/pgo_training.py
    pgomgr /merge python\\src\\hydrobricks\\hydrobricks!1.pgc ^
        build\\pgo\\hydrobricks.pgd
    cmake -S . -B build -DUSE_PGO=use
    cmake --build build --target _hydrobricks --config Release

Only the link is redone in each pass, because MSVC instruments and generates the
final code at link time. Two wrinkles of the instrumented pass, both gone once
the profile is applied: the module then needs ``pgort140.dll`` from the MSVC
toolchain next to it, and the instrumentation keeps alive references that the
link-time optimization would otherwise drop, so a dependency such as
``yaml-cpp.dll`` may have to be copied next to the module as well (this does not
arise with the static vcpkg triplet the wheels are built with). The ``.pgc``
files are written next to the module; set ``VCPROFILE_PATH`` before running this
script to collect them elsewhere.

With GCC the profile is a compile-time input, so both passes recompile::

    cmake -S . -B build -DUSE_PGO=generate && cmake --build build
    python ci/pgo_training.py
    cmake -S . -B build -DUSE_PGO=use && cmake --build build
"""

import tempfile
from pathlib import Path

import hydrobricks as hb
import hydrobricks.models as models

CATCHMENT = (
    Path(__file__).resolve().parent.parent
    / "tests"
    / "files"
    / "catchments"
    / "ch_sitter_appenzell"
)
START_DATE = "1981-01-01"
END_DATE = "2000-12-31"
REFERENCE_ELEVATION = 1250


def _build_forcing(hydro_units, parameters):
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        CATCHMENT / "meteo.csv",
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
        ref_elevation=REFERENCE_ELEVATION,
        gradient=parameters.get("temp_gradients"),
    )
    forcing.spatialize_from_station_data(variable="pet")
    forcing.correct_station_data(
        variable="precipitation",
        correction_factor=parameters.get("precip_correction_factor"),
    )
    forcing.spatialize_from_station_data(
        variable="precipitation",
        ref_elevation=REFERENCE_ELEVATION,
        gradient=parameters.get("precip_gradient"),
    )
    return forcing


def _run_socont(solver, soil_storage_nb, response_factor):
    model = models.Socont(
        soil_storage_nb=soil_storage_nb,
        surface_runoff="linear_storage",
        solver=solver,
    )
    parameters = model.generate_parameters()
    values = {
        "a_snow": 3,
        "k_quick": response_factor,
        "A": 200,
        "k_slow": 0.001,
    }
    if soil_storage_nb == 2:
        values["k_slow_2"] = 0.0005
        values["percol"] = 1.0
    parameters.set_values(values)
    parameters.add_data_parameter("precip_correction_factor", 1)
    parameters.add_data_parameter("precip_gradient", 0.05)
    parameters.add_data_parameter("temp_gradients", -0.6)

    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        CATCHMENT / "hydro_units_elevation.csv",
        column_elevation="elevation",
        column_area="area",
    )
    forcing = _build_forcing(hydro_units, parameters)

    # The core keeps its log file open for the lifetime of the process, so the
    # directory cannot always be removed on the spot.
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as output_path:
        model.setup(
            spatial_structure=hydro_units,
            output_path=output_path,
            start_date=START_DATE,
            end_date=END_DATE,
        )
        model.run(parameters=parameters, forcing=forcing)
        return model.get_total_outlet_discharge()


def main():
    # The default solver carries the most weight, at both a slow and a fast
    # reservoir so that both the affine shortcut and the iterative fallback of
    # the sequential solvers are exercised. The explicit scheme is included
    # because it is the other one users select deliberately.
    workload = [
        ("crank_nicolson", 1, 0.05),
        ("crank_nicolson", 1, 0.8),
        ("crank_nicolson", 2, 0.05),
        ("heun_explicit", 1, 0.05),
    ]
    for solver, soil_storage_nb, response_factor in workload:
        total = _run_socont(solver, soil_storage_nb, response_factor)
        print(
            f"{solver} soil_storage_nb={soil_storage_nb} "
            f"k_quick={response_factor}: total discharge {total:.1f} mm"
        )


if __name__ == "__main__":
    main()
