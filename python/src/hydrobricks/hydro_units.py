import numpy as np
import pandas as pd

import hydrobricks as hb
from _hydrobricks import SettingsBasin

from .units import Unit, convert_unit_df, get_unit_enum, get_unit_from_df_column


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
        for item in land_cover_names:
            land_cover_cols.append((f'{self.prefix_fraction}{item}', 'fraction'))
        if data is not None:
            self._set_units_data(data)
        else:
            self.hydro_units = pd.DataFrame(columns=land_cover_cols)

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

        self.add_property(('id', '-'), range(1, 1 + len(file_content)), set_first=True)

        if column_elevation is not None:
            vals, unit = self._get_column_values_unit(column_elevation, file_content)
            self.add_property(('elevation', unit), vals)
        elif 'elevation' in file_content.columns:
            vals, unit = self._get_column_values_unit('elevation', file_content)
            self.add_property(('elevation', unit), vals)

        if column_area is not None:
            vals, unit = self._get_column_values_unit(column_area, file_content)
            self.add_property(('area', unit), vals)
        elif 'area' in file_content.columns:
            vals, unit = self._get_column_values_unit('area', file_content)
            self.add_property(('area', unit), vals)

        if other_columns is not None:
            for prop, col in other_columns.items():
                self.add_property((prop[0], prop[1]), col)

        if column_fractions is not None:
            raise NotImplementedError

        if columns_areas is not None:
            self._check_land_cover_areas_match(columns_areas)
            area_values = np.zeros(shape=(len(file_content), len(columns_areas)))
            area_unit = None
            for idx, cover in enumerate(self.land_cover_names):
                area_values[:, idx] = file_content[columns_areas[cover]].values[:, 0]
                area_unit_idx = file_content[columns_areas[cover]].columns.values[0]
                if area_unit is None:
                    area_unit = area_unit_idx
                elif area_unit != area_unit_idx:
                    raise ValueError('The area units do not match.')
            self._compute_area_portions(area_values, area_unit)
        else:
            idx = self.prefix_fraction + 'ground'
            self.hydro_units[idx] = np.ones(len(self.hydro_units['area']))

        if get_unit_from_df_column(self.hydro_units['area']) != Unit.M2:
            new_area = convert_unit_df(self.hydro_units['area'], Unit.M2)
            area_idx = self.hydro_units.columns.get_loc('area')
            self.hydro_units.drop(self.hydro_units.columns[area_idx], axis=1,
                                  inplace=True)
            self.hydro_units[('area', 'm2')] = new_area

        self._populate_binding_instance()

    def save_to_csv(self, path):
        """
        Save the hydro units to a csv file.

        Parameters
        ----------
        path : str|Path
            Path to the output file.
        """
        if self.hydro_units is None:
            raise ValueError("No hydro units to save.")

        # Save to csv file with units in the header
        self.hydro_units.to_csv(path, header=True, index=False)

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

    def has(self, prop):
        """
        Check if the hydro units have a given property.

        Parameters
        ----------
        prop : str
            The property to check.

        Returns
        -------
        bool
            True if the property is present, False otherwise.
        """
        return prop in self.hydro_units.columns

    def get_ids(self):
        """
        Get the hydro unit ids.
        """
        return self.hydro_units['id']

    def add_property(self, column_tuple, values, set_first=False):
        df = pd.DataFrame(values, columns=pd.MultiIndex.from_tuples(
            [column_tuple], names=['Property', 'Unit']))

        if self.hydro_units is None:
            self.hydro_units = df
        else:
            if set_first:
                self.hydro_units = pd.concat([df, self.hydro_units], axis=1)
            else:
                self.hydro_units = pd.concat([self.hydro_units, df], axis=1)

    def check_land_cover_fractions_not_empty(self):
        """
        Check that the land cover fractions are not empty. If there is a single one
        (e.g. ground), set them to 1.
        """
        if len(self.land_cover_names) == 1:
            self.hydro_units[self.prefix_fraction + self.land_cover_names[0]] = 1
            return

        for cover_name in self.land_cover_names:
            field_name = self.prefix_fraction + cover_name
            if self.hydro_units[field_name].isnull().values.any():
                raise ValueError(f'The land cover "{cover_name}" contains NaN values.')

    def _populate_binding_instance(self):
        # List properties to be set
        properties = []
        for prop in self.hydro_units.columns.tolist():
            if prop[0] in ['id', 'area']:
                continue
            if 'fraction-' in prop[0]:
                continue
            properties.append(prop[0])

        for _, row in self.hydro_units.iterrows():
            self.settings.add_hydro_unit(int(row['id'].values),
                                         float(row['area'].values))
            for prop in properties:
                if isinstance(row[prop].values, str):
                    self.settings.add_hydro_unit_property_str(prop, row[prop].values)
                else:
                    unit = self._get_unit(row[prop])
                    self.settings.add_hydro_unit_property_double(
                        prop, float(row[prop].values), unit)
            for cover_type, cover_name in zip(self.land_cover_types,
                                              self.land_cover_names):
                fraction = float(row[self.prefix_fraction + cover_name].values)
                self.settings.add_land_cover(cover_name, cover_type, fraction)

    @staticmethod
    def _check_column_names(file_content):
        # Rename unnamed columns to 'No Unit'
        new_column_names = []
        for i, col in enumerate(file_content.columns):
            name = col[0]
            if name is None or name in ['-', '', ' '] or 'Unnamed' in name:
                name = f'{i}'
            unit = str(get_unit_enum(col[1]))
            new_column_names.append((name, unit))

        file_content.columns = pd.MultiIndex.from_tuples(new_column_names)

    @staticmethod
    def _get_column_values_unit(column_name, df):
        col = df.loc[:, column_name]
        unit = col.columns.values[0]
        return col.values.flatten(), unit

    @staticmethod
    def _get_unit(prop):
        return str(prop.index[0])

    def _check_land_cover_areas_match(self, columns_areas):
        if len(columns_areas) != len(self.land_cover_names):
            raise ValueError('The length of the provided "columns_areas" do not match '
                             'the size ot the land cover names.')
        for col in columns_areas:
            if col not in self.land_cover_names:
                raise ValueError(f'The land cover "{col}" was not found in the '
                                 f'defined land covers.')

    def _compute_area_portions(self, area_values, area_unit):
        # Compute total area
        area = np.sum(area_values, axis=1)

        # Normalize land covers
        fractions = area_values / area[:, None]

        # Insert the results in the dataframe
        self.hydro_units[('area', area_unit)] = area
        for idx, cover_name in enumerate(self.land_cover_names):
            field_name = self.prefix_fraction + cover_name
            self.hydro_units[(field_name, 'fraction')] = fractions[:, idx]

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
