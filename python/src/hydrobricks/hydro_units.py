import numpy as np
import pandas as pd
from _hydrobricks import SettingsBasin
from netCDF4 import Dataset


class HydroUnits:
    """Class for the hydro units"""

    def __init__(self, surface_types=None, surface_names=None):
        self.settings = SettingsBasin()
        self._check_surfaces_definitions(surface_types, surface_names)
        self.surface_types = surface_types
        self.surface_names = surface_names
        self.prefix_fraction = 'fraction-'
        surface_cols = []
        if surface_names:
            surface_cols = [f'{self.prefix_fraction}{item}' for item in surface_names]
        columns = ['id', 'area', 'elevation'] + surface_cols
        self.hydro_units = pd.DataFrame(columns=columns)

    def load_from_csv(self, path, area_unit, column_elevation=None, column_area=None,
                      column_fractions=None, columns_areas=None):
        """
        Read hydro units properties from csv file.

        Parameters
        ----------
        path : str
            Path to the csv file containing hydro units data.
        area_unit: str
            Unit for the area values: "m" or "km"
        column_elevation : str
            Column name containing the elevation values in [m] (optional).
        column_area : str
            Column name containing the area values (optional).
        column_fractions : dict
            Column name containing the area fraction values for each surface (optional).
        columns_areas : dict
            Column name containing the area values for each surface (optional).
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
            self._check_surface_areas_match(columns_areas)
            area_values = np.zeros(shape=(len(file_content), len(columns_areas)))
            for idx, surface in enumerate(self.surface_names):
                area_values[:, idx] = file_content[columns_areas[surface]]
            self._compute_area_portions(area_values)

        if area_unit == 'm':
            pass
        elif area_unit == 'km':
            self.hydro_units['area'] = self.hydro_units['area'] * 1000 * 1000
        else:
            raise Exception(f'Unit "{area_unit}" for the area is not supported.')

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

        # Create netCDF file
        nc = Dataset(path, 'w', 'NETCDF4')

        # Global attributes
        nc.version = 1.0
        nc.surface_names = self.surface_names

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

        for surface_type, surface_name in zip(self.surface_types, self.surface_names):
            var_surf = nc.createVariable(surface_name, 'float32', ('hydro_units',))
            var_surf[:] = self.hydro_units[self.prefix_fraction + surface_name]
            var_surf.units = 'fraction'
            var_surf.type = surface_type

        nc.close()

    def get_ids(self):
        """
        Get the hydro unit ids.
        """
        return self.hydro_units['id']

    def _populate_binding_instance(self):
        for _, row in self.hydro_units.iterrows():
            self.settings.add_hydro_unit(int(row['id']), row['area'], row['elevation'])
            for surf_type, surf_name in zip(self.surface_types, self.surface_names):
                fraction = row[self.prefix_fraction + surf_name]
                self.settings.add_surface_element(surf_name, surf_type, fraction)

    def _check_surface_areas_match(self, columns_areas):
        if len(columns_areas) != len(self.surface_names):
            raise Exception('The length of the provided "columns_areas" do not match '
                            'the size ot the surface names.')
        for col in columns_areas:
            if col not in self.surface_names:
                raise Exception(f'The surface "{col}" was not found in the '
                                f'defined surfaces.')

    def _compute_area_portions(self, area_values):
        # Compute total area
        area = np.sum(area_values, axis=1)

        # Normalize surfaces
        fractions = area_values / area[:, None]

        # Insert the results in the dataframe
        self.hydro_units['area'] = area
        for idx, surface_name in enumerate(self.surface_names):
            self.hydro_units[self.prefix_fraction + surface_name] = fractions[:, idx]

    @staticmethod
    def _check_surfaces_definitions(surface_types, surface_names):
        if surface_types is None and surface_names is None:
            return

        if surface_types is None or surface_names is None:
            raise Exception('The surface name or type is undefined.')

        if len(surface_types) != len(surface_names):
            raise Exception('The length of the surface types and names do not match.')
