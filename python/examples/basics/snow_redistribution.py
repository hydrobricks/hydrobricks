import os.path
import tempfile
from pathlib import Path

import hydrobricks as hb
import hydrobricks.models as models
import hydrobricks.plotting.plot_results as plotting

# Paths
CATCHMENT_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '..', '..', '..', 'tests', 'files', 'catchments', 'ch_rhone_gletsch'
)
CATCHMENT_DIR = CATCHMENT_DIR
CATCHMENT_BANDS = CATCHMENT_DIR / 'hydro_units_elevation_radiation.csv'
CATCHMENT_METEO = CATCHMENT_DIR / 'meteo.csv'
CATCHMENT_DISCHARGE = CATCHMENT_DIR / 'discharge.csv'
CATCHMENT_RASTER = CATCHMENT_DIR / 'unit_ids_radiation.tif'
CATCHMENT_CONNECTIVITY = CATCHMENT_DIR / 'connectivity_elevation_radiation.csv'
DEM_RASTER = CATCHMENT_DIR / 'dem.tif'

for with_snow_redistribution in [True, False]:
    with tempfile.TemporaryDirectory() as tmp_dir_name:
        tmp_dir = tmp_dir_name

    os.mkdir(tmp_dir)
    working_dir = Path(tmp_dir)

    if with_snow_redistribution:
        print("Running model with snow redistribution")
        title = "With snow redistribution"
    else:
        print("Running model without snow redistribution")
        title = "Without snow redistribution (snow towers !)"

    # Model structure
    if with_snow_redistribution:
        socont = models.Socont(
            soil_storage_nb=2,
            surface_runoff="linear_storage",
            record_all=True,
            snow_redistribution='transport:snow_slide',
        )
    else:
        socont = models.Socont(
            soil_storage_nb=2,
            surface_runoff="linear_storage",
            record_all=True
        )

    # Parameters
    parameters = socont.generate_parameters()
    parameters.set_values({
        'A': 300,
        'a_snow': 4,
        'k_slow_1': 0.9,
        'k_slow_2': 0.8,
        'k_quick': 1,
        'percol': 9.8
    })

    # Hydro units
    hydro_units = hb.HydroUnits()
    hydro_units.load_from_csv(
        CATCHMENT_BANDS,
        column_elevation='elevation',
        column_area='area',
        other_columns={'slope': 'slope'}
    )

    # Load connectivity (computed as in compute_lateral_connectivity.py)
    if with_snow_redistribution:
        hydro_units.set_connectivity(CATCHMENT_CONNECTIVITY)

    # Meteo data
    ref_elevation = 2702  # Reference altitude for the meteo data
    forcing = hb.Forcing(hydro_units)
    forcing.load_station_data_from_csv(
        CATCHMENT_METEO,
        column_time='date',
        time_format='%d/%m/%Y',
        content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)'}
    )

    forcing.spatialize_from_station_data(
        variable='temperature',
        ref_elevation=ref_elevation,
        gradient=-0.6
    )
    forcing.spatialize_from_station_data(
        variable='precipitation',
        ref_elevation=ref_elevation,
        gradient=0.05
    )
    forcing.compute_pet(
        method='Hamon',
        use=['t', 'lat'],
        lat=46.6
    )

    # Obs data
    obs = hb.Observations()
    obs.load_from_csv(
        CATCHMENT_DISCHARGE,
        column_time='Date',
        time_format='%d/%m/%Y',
        content={'discharge': 'Discharge (mm/d)'}
    )

    # Model setup
    socont.setup(
        spatial_structure=hydro_units,
        output_path=str(working_dir),
        start_date='1981-01-01',
        end_date='2020-12-31'
    )

    # Run the model
    socont.run(
        parameters=parameters,
        forcing=forcing
    )

    # Dump all outputs
    socont.dump_outputs(str(working_dir))

    # Load the netcdf file
    results = hb.Results(str(working_dir) + '/results.nc')

    # List the hydro units components available
    results.list_hydro_units_components()

    # Plot the snow water equivalent on a map
    plotting.plot_map_hydro_unit_value(
        results,
        CATCHMENT_RASTER,
        "ground_snowpack:snow_content",
        "2020-12-31",
        dem_path=DEM_RASTER,
        figsize=(5, 6),
        title=title,
    )

    # Create an animated map of the snow water equivalent
    plotting.create_animated_map_hydro_unit_value(
        results, CATCHMENT_RASTER,
        "ground_snowpack:snow_content",
        "1990-01-01",
        "1990-03-20",
        save_path=str(working_dir),
        dem_path=DEM_RASTER,
        figsize=(5, 6),
    )

