import pint
import pint_pandas
import numpy as np
import pandas as pd

import hydrobricks as hb
from _hydrobricks import SettingsBasin
from hydrobricks import utils

ureg = pint.UnitRegistry()
pint_pandas.PintType.ureg.default_format = "P~"


class HydroUnits:
    """Class for the hydro units"""

    def __init__(self, land_cover_types=None, land_cover_names=None, data=None):
        self.settings = SettingsBasin()
        self._check_land_cover_definitions(land_cover_types, land_cover_names)
        if not land_cover_types:
            land_cover_types = ['ground']
        if not land_cover_names:
            land_cover_names = ['ground']
        self.land_cover_types = land_cover_types
        self.land_cover_names = land_cover_names
        self.prefix_fraction = 'fraction-'
        land_cover_cols = []
        if land_cover_names:
            land_cover_cols = [f'{self.prefix_fraction}{item}' for item in
                               land_cover_names]
        if data is not None:
            self._set_units_data(data)
        else:
            columns = ['id', 'area', 'elevation'] + land_cover_cols
            self.hydro_units = pd.DataFrame(columns=columns)

    def load_from_csv(self, path, column_elevation=None, column_area=None,
                      column_fractions=None, columns_areas=None, other_columns=None):
        """
        Read hydro units properties from csv file. The file must contain two header
        rows. The first row contains the column names and the second row contains the
        units. The file must contain at a minimum the units area.

        Parameters
        ----------
        path : str|Path
            Path to the csv file containing hydro units data.
        column_elevation : str, optional
            Column name containing the elevation values (optional).
            Default: elevation
        column_area : str, optional
            Column name containing the area values (optional).
            Default: area
        column_fractions : dict, optional
            Column name containing the area fraction values for each land cover
            (optional).
        columns_areas : dict, optional
            Column name containing the area values for each land cover (optional).
        other_columns: dict, optional
            Column name containing other values to import (optional). The key is the
            property name and the value is the name of the column in the csv file.
            Example: {'slope': 'Slope', 'aspect': 'Aspect'}
        """
        file_content = pd.read_csv(path, header=[0, 1])
        self._check_column_names(file_content)
        file_content = file_content.pint.quantify(level=-1)

        self.hydro_units['id'] = range(1, 1 + len(file_content))

        if column_elevation is not None:
            self.hydro_units['elevation'] = file_content[column_elevation]
        elif 'elevation' in file_content.columns:
            self.hydro_units['elevation'] = file_content['elevation']

        if column_area is not None:
            self.hydro_units['area'] = file_content[column_area]
        elif 'area' in file_content.columns:
            self.hydro_units['area'] = file_content['area']

        if other_columns is not None:
            for prop, col in other_columns.items():
                self.hydro_units[prop] = file_content[col]

        if column_fractions is not None:
            raise NotImplementedError

        if columns_areas is not None:
            self._check_land_cover_areas_match(columns_areas)
            area_values = np.zeros(shape=(len(file_content), len(columns_areas)))
            for idx, cover in enumerate(self.land_cover_names):
                area_values[:, idx] = file_content[columns_areas[cover]]
            self._compute_area_portions(area_values)
        else:
            idx = self.prefix_fraction + 'ground'
            self.hydro_units[idx] = np.ones(len(self.hydro_units['area']))

        self.hydro_units['area'] = self.hydro_units['area'].pint.to('m^2')

        self._populate_binding_instance()

    def save_as(self, path):
        """
        Create a file containing the hydro unit properties. Such a file can be used in
        the command-line version of hydrobricks.

        Parameters
        ----------
        path : str
            Path of the file to create.
        """
        if not hb.has_netcdf:
            raise ImportError("netcdf4 is required to do this.")

        # Create netCDF file
        nc = hb.Dataset(path, 'w', 'NETCDF4')

        # Global attributes
        nc.version = 1.0
        nc.land_cover_names = self.land_cover_names

        # Dimensions
        nc.createDimension('hydro_units', len(self.hydro_units))

        # Variables
        var_id = nc.createVariable('id', 'int', ('hydro_units',))
        var_id[:] = self.hydro_units['id']

        var_area = nc.createVariable('area', 'float32', ('hydro_units',))
        var_area[:] = self.hydro_units['area']
        var_area.units = 'm2'

        var_elevation = nc.createVariable('elevation', 'float32', ('hydro_units',))
        var_elevation[:] = self.hydro_units['elevation']
        var_elevation.units = 'm'

        for cover_type, cover_name in zip(self.land_cover_types, self.land_cover_names):
            var_cover = nc.createVariable(cover_name, 'float32', ('hydro_units',))
            var_cover[:] = self.hydro_units[self.prefix_fraction + cover_name]
            var_cover.units = 'fraction'
            var_cover.type = cover_type

        nc.close()

    def get_ids(self):
        """
        Get the hydro unit ids.
        """
        return self.hydro_units['id']

    def _populate_binding_instance(self):
        # List properties to be set
        properties = self.hydro_units.columns.tolist()
        properties = [p for p in properties if p not in ['id', 'area']]
        properties = [p for p in properties if 'fraction-' not in p]

        for _, row in self.hydro_units.iterrows():
            self.settings.add_hydro_unit(int(row['id']), row['area'].magnitude)
            for prop in properties:
                if isinstance(row[prop], str):
                    self.settings.add_hydro_unit_property_str(prop, row[prop])
                else:
                    self.settings.add_hydro_unit_property_double(
                        prop, row[prop].magnitude, self._get_unit(row[prop]))
            for cover_type, cover_name in zip(self.land_cover_types,
                                              self.land_cover_names):
                fraction = row[self.prefix_fraction + cover_name]
                self.settings.add_land_cover(cover_name, cover_type, fraction)

    def _check_column_names(self, file_content):
        # Rename unnamed columns to 'No Unit'
        new_column_names = []
        for i, col in enumerate(file_content.columns):
            if 'Unnamed' in col[0] or 'Unnamed' in col[1]:
                new_column_names.append(('No Name', 'No Unit'))
            else:
                self._check_unit(col[1])
                new_column_names.append(col)

        file_content.columns = pd.MultiIndex.from_tuples(new_column_names)

    @staticmethod
    def _check_unit(unit):
        if unit not in ureg:
            raise ValueError(f'The unit "{unit}" is not recognized by pint.')

    @staticmethod
    def _get_unit(prop):
        return str(prop.units) if hasattr(prop, 'units') else None

    def _check_land_cover_areas_match(self, columns_areas):
        if len(columns_areas) != len(self.land_cover_names):
            raise ValueError('The length of the provided "columns_areas" do not match '
                             'the size ot the land cover names.')
        for col in columns_areas:
            if col not in self.land_cover_names:
                raise ValueError(f'The land cover "{col}" was not found in the '
                                 f'defined land covers.')

    def _compute_area_portions(self, area_values):
        # Compute total area
        area = np.sum(area_values, axis=1)

        # Normalize land covers
        fractions = area_values / area[:, None]

        # Insert the results in the dataframe
        self.hydro_units['area'] = area
        for idx, cover_name in enumerate(self.land_cover_names):
            self.hydro_units[self.prefix_fraction + cover_name] = fractions[:, idx]

    def _set_units_data(self, data):
        assert isinstance(data, pd.DataFrame)
        assert 'area' in data.columns
        assert 'elevation' in data.columns

        if 'id' not in data.columns:
            data['id'] = range(1, 1 + len(data))

        self.hydro_units = data
        idx = self.prefix_fraction + 'ground'
        self.hydro_units[idx] = np.ones(len(self.hydro_units['area']))
        self._populate_binding_instance()

    @staticmethod
    def _check_land_cover_definitions(land_cover_types, land_cover_names):
        if land_cover_types is None and land_cover_names is None:
            return

        if land_cover_types is None or land_cover_names is None:
            raise ValueError('The land cover name or type is undefined.')

        if len(land_cover_types) != len(land_cover_names):
            raise ValueError('The length of the land cover types & names do not match.')
