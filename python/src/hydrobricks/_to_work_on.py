import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

from netCDF4 import Dataset


def create_units_file(path, hydro_units, surface_names=None,
                      surface_types=None):
    file_path = path / 'spatial_structure.nc'

    # Create netCDF file
    nc = Dataset(file_path, 'w', 'NETCDF4')

    # Global attributes
    nc.version = 1.0
    nc.surface_names = surface_names

    # Dimensions
    nc.createDimension('hydro_units', len(hydro_units))

    # Variables
    var_id = nc.createVariable('id', 'int', ('hydro_units',))
    var_id[:] = hydro_units['id']

    var_area = nc.createVariable('area', 'float32', ('hydro_units',))
    var_area[:] = hydro_units['area'] * 1000 * 1000
    var_area.units = 'm2'

    var_elevation = nc.createVariable('elevation', 'float32', ('hydro_units',))
    var_elevation[:] = hydro_units['elevation']
    var_elevation.units = 'm'

    if surface_names:
        for i in range(len(surface_names)):
            var_surf = nc.createVariable(surface_names[i], 'float32', ('hydro_units',))
            var_surf[:] = hydro_units[surface_names[i]]
            var_surf.units = 'fraction'
            var_surf.type = surface_types[i]

    nc.close()


def create_forcing_file(path, hydro_units, time, forcing_data,
                        forcing_variables, max_compression=False):
    file_path = path / 'time_series.nc'

    # Create netCDF file
    nc = Dataset(file_path, 'w', 'NETCDF4')

    # Dimensions
    nc.createDimension('hydro_units', len(hydro_units))
    nc.createDimension('time', forcing_data[0].shape[0])

    # Variables
    var_id = nc.createVariable('id', 'int', ('hydro_units',))
    var_id[:] = hydro_units['id']

    var_time = nc.createVariable('time', 'float32', ('time',))
    var_time[:] = time
    var_time.units = 'days since 1858-11-17 00:00:00'
    var_time.comment = 'Modified Julian Day Numer'

    for index, variable in enumerate(forcing_variables):
        if max_compression:
            var_data = nc.createVariable(variable, 'float32',
                                         ('time', 'hydro_units'), zlib=True,
                                         least_significant_digit=3)
        else:
            var_data = nc.createVariable(variable, 'float32',
                                         ('time', 'hydro_units'), zlib=True)
        var_data[:, :] = forcing_data[index]

    nc.close()


def extract_time_as_mjd(ts):
    return pd.DatetimeIndex(ts['date']).to_julian_date() - 2400000.5


def compute_area_portions(hydro_units, surface_names):
    # Compute total area
    area = 0
    for surface in surface_names:
        area += hydro_units[surface]

    # Normalize surfaces
    for surface in surface_names:
        hydro_units[surface] /= area

    # Insert the total area in the dataframe
    hydro_units.insert(loc=1, column='area', value=area)


def create_ids(hydro_units):
    hydro_units.insert(0, 'id', range(1, 1 + len(hydro_units)))


def spatialize_temp(ts, hydro_units, reference_alt, temp_lapse):
    units_temperature = np.zeros((len(ts), len(hydro_units)))
    hydro_units = hydro_units.reset_index()
    for index, unit in hydro_units.iterrows():
        elevation = unit['elevation']
        if len(temp_lapse) == 1:
            units_temperature[:, index] = ts['temperature'] + temp_lapse * \
                                          (elevation - reference_alt) / 100
        elif len(temp_lapse) == 12:
            for m in range(12):
                month = ts.date.dt.month == m + 1
                month = month.to_numpy()
                units_temperature[month, index] = \
                    ts['temperature'][month] + temp_lapse[m] * \
                    (elevation - reference_alt) / 100
        else:
            raise ValueError(
                f'The temperature lapse should have a length of 1 or 12. '
                f'Here: {len(temp_lapse)}')

    return units_temperature


def spatialize_precip(ts, hydro_units, reference_alt, precip_corr_1, precip_corr_2=None,
                      precip_corr_change=None):
    units_precip = np.zeros((len(ts), len(hydro_units)))
    hydro_units = hydro_units.reset_index()
    for index, unit in hydro_units.iterrows():
        elevation = unit['elevation']

        if not precip_corr_change:
            units_precip[:, index] = ts['precipitation'] * (
                        1 + precip_corr_1 * (elevation - reference_alt) / 100)
            continue

        if elevation < precip_corr_change:
            units_precip[:, index] = ts['precipitation'] * (
                        1 + precip_corr_1 * (elevation - reference_alt) / 100)
        else:
            precip_below = ts['precipitation'] * (1 + precip_corr_1 * (
                        precip_corr_change - reference_alt) / 100)
            units_precip[:, index] = precip_below * (1 + precip_corr_2 * (
                        elevation - precip_corr_change) / 100)

    return units_precip


def spatialize_pet(ts, hydro_units, reference_alt, pet_gradient):
    units_pet = np.zeros((len(ts), len(hydro_units)))
    hydro_units = hydro_units.reset_index()
    for index, unit in hydro_units.iterrows():
        elevation = unit['elevation']
        if len(pet_gradient) == 1:
            if pet_gradient == 0:
                units_pet[:, index] = ts['pet']
            else:
                units_pet[:, index] = ts['pet'] + pet_gradient * \
                                      (elevation - reference_alt) / 100
        elif len(pet_gradient) == 12:
            for m in range(12):
                month = ts.date.dt.month == m + 1
                month = month.to_numpy()
                units_pet[month, index] = \
                    ts['pet'][month] + pet_gradient[m] * \
                    (elevation - reference_alt) / 100
        else:
            raise ValueError(
                f'The temperature lapse should have a length of 1 or 12. '
                f'Here: {len(pet_gradient)}')

    return units_pet



def plot_precip_per_unit(units_precip, hydro_units):
    sum_precip = units_precip.sum(axis=0)

    plt.figure()
    x = hydro_units['elevation'].tolist()
    y = sum_precip.astype('int').tolist()
    plt.plot(x, y)
    plt.xlabel('elevation [m]')
    plt.ylabel('precipitation total [mm]')
    plt.tight_layout()
    plt.show()


def plot_hydro_units_values(results, index, units, units_labels):
    plt.figure()
    legend = []
    for unit in units:
        results.hydro_units_values.loc[index].loc[unit].plot.line(x="time")
        legend.append(units_labels[index] + f"({unit})")
    plt.legend(legend)
    plt.tight_layout()
    plt.show()


