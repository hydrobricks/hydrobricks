import numpy as np
from netCDF4 import Dataset

from .time_series import TimeSeries


class Forcing(TimeSeries):
    """Class for forcing data"""

    def __init__(self, hydro_units):
        super().__init__()
        self.hydro_units = hydro_units.hydro_units

    def spatialize_temperature(self, ref_elevation, lapse):
        """
        Spatializes the temperature using a temperature lapse that is either constant
        or changes for every month.

        Parameters
        ----------
        ref_elevation : float
            Elevation of the reference station.
        lapse : float/list
            Temperature lapse [°C/100m] to compute the temperature for every hydro unit.
            Can be a unique value or a list providing a value for every month.
        """
        unit_values = np.zeros((len(self.time), len(self.hydro_units)))
        hydro_units = self.hydro_units.reset_index()
        i_col = self.data_name.index('temperature')
        for i_unit, unit in hydro_units.iterrows():
            elevation = unit['elevation']

            if len(lapse) == 1:
                unit_values[:, i_unit] = self.data_raw[i_col] + lapse * \
                                              (elevation - ref_elevation) / 100
            elif len(lapse) == 12:
                for m in range(12):
                    month = self.time.dt.month == m + 1
                    month = month.to_numpy()
                    unit_values[month, i_unit] = \
                        self.data_raw[i_col][month] + lapse[m] * \
                        (elevation - ref_elevation) / 100
            else:
                raise ValueError(
                    f'The temperature lapse should have a length of 1 or 12. '
                    f'Here: {len(lapse)}')

        self.data_spatialized[i_col] = unit_values

    def spatialize_pet(self, ref_elevation, gradient=0):
        """
        Spatializes the PET using a gradient that is either constant or changes for
        every month.

        Parameters
        ----------
        ref_elevation : float
            Elevation of the reference station.
        gradient : float/list
            Gradient [mm/100m] to compute the PET for every hydro unit.
            Can be a unique value or a list providing a value for every month.
        """
        unit_values = np.zeros((len(self.time), len(self.hydro_units)))
        hydro_units = self.hydro_units.reset_index()
        i_col = self.data_name.index('pet')
        for i_unit, unit in hydro_units.iterrows():
            elevation = unit['elevation']

            if len(gradient) == 1:
                if gradient == 0:
                    unit_values[:, i_unit] = self.data_raw[i_col]
                else:
                    unit_values[:, i_unit] = self.data_raw[i_col] + gradient * \
                                             (elevation - ref_elevation) / 100

            elif len(gradient) == 12:
                for m in range(12):
                    month = self.time.dt.month == m + 1
                    month = month.to_numpy()
                    unit_values[month, i_unit] = \
                        self.data_raw[i_col][month] + gradient[m] * \
                        (elevation - ref_elevation) / 100
            else:
                raise ValueError(
                    f'The PET gradient should have a length of 1 or 12. '
                    f'Here: {len(gradient)}')

        unit_values[unit_values < 0] = 0

        self.data_spatialized[i_col] = unit_values

    def spatialize_precipitation(self, ref_elevation, gradient_1, gradient_2=None,
                                 elevation_threshold=None):
        """
        Spatializes the precipitation using a single gradient for the full elevation
        range or a two-gradients approach with an elevation threshold.

        Parameters
        ----------
        ref_elevation : float
            Elevation of the reference station.
        gradient_1 : float
            Precipitation gradient (ratio) per 100 m of altitude.
        gradient_2 : float
            Precipitation gradient (ratio) per 100 m of altitude for the units above
            the threshold elevation (optional).
        elevation_threshold: float
            Threshold to switch from gradient 1 to gradient 2 (optional).
        """
        unit_values = np.zeros((len(self.time), len(self.hydro_units)))
        hydro_units = self.hydro_units.reset_index()
        i_col = self.data_name.index('precipitation')
        for i_unit, unit in hydro_units.iterrows():
            elevation = unit['elevation']

            if not elevation_threshold:
                unit_values[:, i_unit] = self.data_raw[i_col] * (
                            1 + gradient_1 * (elevation - ref_elevation) / 100)
                continue

            if elevation < elevation_threshold:
                unit_values[:, i_unit] = self.data_raw[i_col] * (
                            1 + gradient_1 * (elevation - ref_elevation) / 100)
            else:
                precip_below = self.data_raw[i_col] * (1 + gradient_1 * (
                            elevation_threshold - ref_elevation) / 100)
                unit_values[:, i_unit] = precip_below * (
                            1 + gradient_2 * (elevation - elevation_threshold) / 100)

        self.data_spatialized[i_col] = unit_values

    def create_file(self, path, max_compression=False):
        """
        Read hydro units properties from csv file.

        Parameters
        ----------
        path : str
            Path of the file to create.
        max_compression: bool
            Option to allow maximum compression for data in file.
        """

        time = self._date_as_mjd()

        # Create netCDF file
        nc = Dataset(path, 'w', 'NETCDF4')

        # Dimensions
        nc.createDimension('hydro_units', len(self.hydro_units))
        nc.createDimension('time', len(time))

        # Variables
        var_id = nc.createVariable('id', 'int', ('hydro_units',))
        var_id[:] = self.hydro_units['id']

        var_time = nc.createVariable('time', 'float32', ('time',))
        var_time[:] = time
        var_time.units = 'days since 1858-11-17 00:00:00'
        var_time.comment = 'Modified Julian Day Numer'

        for index, variable in enumerate(self.data_name):
            if max_compression:
                var_data = nc.createVariable(
                    variable, 'float32', ('time', 'hydro_units'), zlib=True,
                    least_significant_digit=3)
            else:
                var_data = nc.createVariable(
                    variable, 'float32', ('time', 'hydro_units'), zlib=True)
            var_data[:, :] = self.data_spatialized[index]

        nc.close()
