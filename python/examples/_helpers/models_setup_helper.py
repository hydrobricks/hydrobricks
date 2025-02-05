import os.path
import shutil
import tempfile
from pathlib import Path

import hydrobricks as hb
import hydrobricks.models as models

# Paths
TEST_FILES_DIR = Path(
    os.path.dirname(os.path.realpath(__file__)),
    '../calibration', '..', '..', '..', 'tests', 'files', 'catchments'
)


class ModelSetupHelper:
    """
    Helper class to setup a model for testing using data provided in the repo.
    This class is used in the examples and only aims at avoiding code duplication.
    You do not need to use it in your own code.
    """

    def __init__(self, catchment_name, start_date, end_date, working_dir=None):
        self.catchment_name = catchment_name
        self.start_date = start_date
        self.end_date = end_date
        self.hydro_units = None
        self.working_dir = None
        if working_dir is not None:
            self.working_dir = Path(working_dir)

        self._create_temp_working_directory()

        # Options defined later
        self.use_precip_gradient = False
        self.precip_corr_factor = None
        self.temp_gradients = None

    def __del__(self):
        # Cleanup
        try:
            shutil.rmtree(self.working_dir)
        except Exception:
            print("Failed to clean up.")

    def get_catchment_dir(self):
        return TEST_FILES_DIR / self.catchment_name

    def create_hydro_units_from_csv_file(self, filename='hydro_units.csv',
                                         other_columns=None):
        catchment_dir = TEST_FILES_DIR / self.catchment_name

        self.hydro_units = hb.HydroUnits()
        self.hydro_units.load_from_csv(
            catchment_dir / filename, column_elevation='elevation', column_area='area',
            other_columns=other_columns)

    def get_forcing_data_from_csv_file(self, filename='meteo.csv', ref_elevation=None,
                                       use_pyet=False, use_precip_gradient=False,
                                       precip_corr_factor=None, temp_gradients=None):
        self.use_precip_gradient = use_precip_gradient
        self.precip_corr_factor = precip_corr_factor
        self.temp_gradients = temp_gradients

        catchment_dir = TEST_FILES_DIR / self.catchment_name

        forcing = hb.Forcing(self.hydro_units)
        forcing.load_station_data_from_csv(
            catchment_dir / filename, column_time='Date', time_format='%d/%m/%Y',
            content={'precipitation': 'precip(mm/day)', 'temperature': 'temp(C)',
                     'pet': 'pet_sim(mm/day)'})

        if temp_gradients is not None:
            forcing.spatialize_from_station_data(
                variable='temperature', ref_elevation=ref_elevation,
                gradient=temp_gradients)
        else:
            forcing.spatialize_from_station_data(
                variable='temperature', ref_elevation=ref_elevation,
                gradient='param:temp_gradients')

        if use_pyet:
            forcing.compute_pet(method='priestley_taylor')
        else:
            forcing.spatialize_from_station_data(variable='pet', method='constant')

        if precip_corr_factor is not None:
            forcing.correct_station_data(
                variable='precipitation', correction_factor=precip_corr_factor
            )
        else:
            forcing.correct_station_data(
                variable='precipitation', correction_factor='param:precip_corr_factor'
            )

        if use_precip_gradient:
            forcing.spatialize_from_station_data(
                variable='precipitation', ref_elevation=ref_elevation,
                gradient='param:precip_gradient',
            )
        else:
            forcing.spatialize_from_station_data(
                variable='precipitation', method='constant'
            )

        return forcing

    def get_obs_data_from_csv_file(self, filename='discharge.csv'):
        catchment_dir = TEST_FILES_DIR / self.catchment_name

        obs = hb.Observations()
        obs.load_from_csv(
            catchment_dir / filename, column_time='Date', time_format='%d/%m/%Y',
            content={'discharge': 'Discharge (mm/d)'}
        )

        return obs

    def get_model_and_params_socont(
            self, soil_storage_nb=2, surface_runoff="linear_storage",
            snow_melt_process='melt:degree_day', record_all=False):

        socont = models.Socont(soil_storage_nb=soil_storage_nb,
                               surface_runoff=surface_runoff,
                               snow_melt_process=snow_melt_process,
                               record_all=record_all)

        socont.setup(spatial_structure=self.hydro_units,
                     output_path=str(self.working_dir),
                     start_date=self.start_date,
                     end_date=self.end_date)

        # Parameters
        parameters = socont.generate_parameters()
        parameters.define_constraint('k_slow_2', '<', 'k_slow_1')
        if self.precip_corr_factor is None:
            parameters.add_data_parameter('temp_gradients', -0.6,
                                          min_value=-1, max_value=0)
        if self.precip_corr_factor is None:
            parameters.add_data_parameter('precip_corr_factor', 0.85,
                                          min_value=0.7, max_value=1.3)
        if self.use_precip_gradient:
            parameters.add_data_parameter('precip_gradient', 0.05,
                                          min_value=0, max_value=0.2)

        return socont, parameters

    def _create_temp_working_directory(self):
        if self.working_dir is not None:
            return

        with tempfile.TemporaryDirectory() as tmp_dir_name:
            tmp_dir = tmp_dir_name
        os.mkdir(tmp_dir)
        self.working_dir = Path(tmp_dir)
