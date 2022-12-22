import numpy as np
import pandas as pd

import hydrobricks as hb
from _hydrobricks import SettingsBasin
from hydrobricks import utils


class HydroUnits:
    """Class for the hydro units"""

    def __init__(self, land_cover_types=None, land_cover_names=None):
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
        columns = ['id', 'area', 'elevation'] + land_cover_cols
        self.hydro_units = pd.DataFrame(columns=columns)

    def load_from_csv(self, path, area_unit, column_elevation=None, column_area=None,
                      column_fractions=None, columns_areas=None):
        """
        Read hydro units properties from csv file.

        Parameters
        ----------
        path : str|Path
            Path to the csv file containing hydro units data.
        area_unit: str
            Unit for the area values: "m2" or "km2"
        column_elevation : str
            Column name containing the elevation values in [m] (optional).
        column_area : str
            Column name containing the area values (optional).
        column_fractions : dict
            Column name containing the area fraction values for each land cover
            (optional).
        columns_areas : dict
            Column name containing the area values for each land cover (optional).
        """
        file_content = pd.read_csv(path)

        self.hydro_units['id'] = range(1, 1 + len(file_content))

        if column_elevation is not None:
            self.hydro_units['elevation'] = file_content[column_elevation]

        if column_area is not None:
            self.hydro_units['area'] = file_content[column_area]

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

        self.hydro_units['area'] = utils.area_in_m2(self.hydro_units['area'], area_unit)

        self._populate_binding_instance()

    def create_file(self, path):
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
        for _, row in self.hydro_units.iterrows():
            self.settings.add_hydro_unit(int(row['id']), row['area'], row['elevation'])
            for cover_type, cover_name in zip(self.land_cover_types,
                                              self.land_cover_names):
                fraction = row[self.prefix_fraction + cover_name]
                self.settings.add_land_cover(cover_name, cover_type, fraction)

    def _check_land_cover_areas_match(self, columns_areas):
        if len(columns_areas) != len(self.land_cover_names):
            raise Exception('The length of the provided "columns_areas" do not match '
                            'the size ot the land cover names.')
        for col in columns_areas:
            if col not in self.land_cover_names:
                raise Exception(f'The land cover "{col}" was not found in the '
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

    @staticmethod
    def _check_land_cover_definitions(land_cover_types, land_cover_names):
        if land_cover_types is None and land_cover_names is None:
            return

        if land_cover_types is None or land_cover_names is None:
            raise Exception('The land cover name or type is undefined.')

        if len(land_cover_types) != len(land_cover_names):
            raise Exception('The length of the land cover types & names do not match.')
