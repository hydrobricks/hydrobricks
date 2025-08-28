import ast
from pathlib import Path

import numpy as np
import pandas as pd

import hydrobricks as hb
from _hydrobricks import SettingsBasin
from hydrobricks.units import (
    Unit,
    convert_unit_df,
    get_unit_enum,
    get_unit_from_df_column,
)


class HydroUnits:
    """
    Class for the hydro units

    Parameters
    ----------
    land_cover_types
        List of land cover types. Default: ['ground']
    land_cover_names
        List of land cover names. Default: ['ground']
    data
        Dataframe containing the hydro units data.

    Attributes
    ----------
    land_cover_types : list
        List of land cover types. Default: ['ground']
    land_cover_names : list
        List of land cover names. Default: ['ground']
    hydro_units : pd.DataFrame
        Dataframe containing the hydro units data.
    """

    def __init__(
            self,
            land_cover_types: list[str] | None = None,
            land_cover_names: list[str] | None = None,
            data: pd.DataFrame | None = None
    ):
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

    def load_from_csv(
            self,
            path: str | Path,
            column_elevation: str | None = None,
            column_area: str | None = None,
            column_fractions: dict | None = None,
            columns_areas: dict | None = None,
            other_columns: dict | None = None
    ):
        """
        Read hydro units properties from csv file. The file must contain two header
        rows. The first row contains the column names and the second row contains the
        units. The file must contain at a minimum the units area.

        Parameters
        ----------
        path
            Path to the csv file containing hydro units data.
        column_elevation
            Column name containing the elevation values.
            Default: elevation
        column_area
            Column name containing the area values.
            Default: area
        column_fractions
            Column name containing the area fraction values for each land cover.
        columns_areas
            Column name containing the area values for each land cover.
        other_columns
            Column name containing other values to import. The key is the property name
            and the value is the name of the column in the csv file.
            Example: {'slope': 'Slope', 'aspect': 'Aspect'}
        """
        file_content = pd.read_csv(path, header=[0, 1])
        self._check_column_names(file_content)

        # Check that the id column is present
        if 'id' not in file_content.columns:
            raise ValueError('The "id" column is required in the file.')
        vals, _ = self._get_column_values_unit('id', file_content)
        self.add_property(('id', '-'), vals, set_first=True)

        if column_elevation is not None:
            vals, unit = self._get_column_values_unit(column_elevation, file_content)
            self.add_property(('elevation', unit), vals)
        elif 'elevation' in file_content.columns:
            vals, unit = self._get_column_values_unit('elevation', file_content)
            self.add_property(('elevation', unit), vals)
        else:
            raise ValueError('The "elevation" column is required in the file.')

        if column_area is not None and columns_areas is not None:
            raise ValueError('The "column_area" and "columns_areas" cannot be '
                             'provided at the same time.')

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
            if column_area is not None:
                vals, unit = self._get_column_values_unit(column_area, file_content)
                self.add_property(('area', unit), vals)
            elif 'area' in file_content.columns:
                vals, unit = self._get_column_values_unit('area', file_content)
                self.add_property(('area', unit), vals)
            idx = self.prefix_fraction + 'ground'
            self.hydro_units[idx] = np.ones(len(self.hydro_units[('area', unit)]))

        if other_columns is not None:
            for prop, col in other_columns.items():
                vals, unit = self._get_column_values_unit(col, file_content)
                self.add_property((prop, unit), vals)

        if column_fractions is not None:
            raise NotImplementedError

        if get_unit_from_df_column(self.hydro_units['area']) != Unit.M2:
            new_area = convert_unit_df(self.hydro_units['area'], Unit.M2)
            area_idx = self.hydro_units.columns.get_loc('area')
            self.hydro_units.drop(
                self.hydro_units.columns[area_idx],
                axis=1,
                inplace=True
            )
            self.hydro_units[('area', 'm2')] = new_area

        self.populate_bounded_instance()

    def save_to_csv(self, path: str | Path):
        """
        Save the hydro units to a csv file.

        Parameters
        ----------
        path
            Path to the output file.
        """
        if self.hydro_units is None:
            raise ValueError("No hydro units to save.")

        # Save to csv file with units in the header
        self.hydro_units.to_csv(path, header=True, index=False)

    def save_as(self, path: str):
        """
        Create a file containing the hydro unit properties. Such a file can be used in
        the command-line version of hydrobricks.

        Parameters
        ----------
        path
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

    def has(self, prop: str) -> bool:
        """
        Check if the hydro units have a given property.

        Parameters
        ----------
        prop
            The property to check.

        Returns
        -------
        True if the property is present, False otherwise.
        """
        return prop in self.hydro_units.columns

    def get_ids(self) -> pd.Series:
        """
        Get the hydro unit ids.
        """
        return self.hydro_units['id']

    def add_property(
            self,
            column_tuple: tuple[str, str],
            values: np.ndarray,
            set_first: bool = False
    ):
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
            self.hydro_units[self.prefix_fraction + self.land_cover_names[0]] = 1.0
            return

        for cover_name in self.land_cover_names:
            field_name = self.prefix_fraction + cover_name
            if self.hydro_units[field_name].isnull().values.any():
                raise ValueError(f'The land cover "{cover_name}" contains NaN values.')

    def initialize_land_cover_fractions(self):
        # Set the land cover fractions of 'ground' to 1 and the rest to 0
        for cover_name in self.land_cover_names:
            field_name = self.prefix_fraction + cover_name
            self.hydro_units[(field_name, 'fraction')] = 0.0
        self.hydro_units[(self.prefix_fraction + 'ground', 'fraction')] = 1.0

    def initialize_from_land_cover_change(
            self,
            land_cover_name: str,
            land_cover_change: pd.DataFrame
    ):
        """
        Initialize the hydro units from the first values of a land cover change
        dataframe.

        Parameters
        ----------
        land_cover_name
            The name of the land cover to initialize.
        land_cover_change
            The land cover change dataframe.
        """
        # Land cover fraction column name
        field_name = self.prefix_fraction + land_cover_name
        ground_name = self.prefix_fraction + 'ground'

        # Apply land cover fractions one hydro unit at a time (order might differ)
        for _, row in land_cover_change.iterrows():
            id = row['hydro_unit']
            land_cover_area = row.iloc[1]

            # Get the hydro unit row
            hu_idx = self.hydro_units[self.hydro_units[('id', '-')] == id].index[0]
            hu_area = self.hydro_units.loc[hu_idx, ('area', 'm2')]

            # Compute the land cover fraction
            fraction = land_cover_area / hu_area
            if not (0 <= fraction <= 1):
                raise ValueError(f'Land cover fraction {fraction} for '
                                 f'hydro unit {id} is not in the range [0, 1].')

            # Set the land cover fraction
            self.hydro_units.loc[hu_idx, (field_name, 'fraction')] = fraction
            self.hydro_units.loc[hu_idx, (ground_name, 'fraction')] -= fraction

        self.populate_bounded_instance()

    def populate_bounded_instance(self):
        self.settings.clear()

        # List properties to be set
        properties = []
        for prop in self.hydro_units.columns.tolist():
            if prop[0] in ['id', 'area', 'elevation']:
                continue
            if 'fraction-' in prop[0]:
                continue
            properties.append(prop[0])

        # Sort the hydro units by decreasing elevation
        hydro_units = self.hydro_units.copy()
        hydro_units.sort_values(
            by=('elevation', 'm'),
            ascending=False,
            inplace=True
        )

        for _, row in hydro_units.iterrows():
            self.settings.add_hydro_unit(
                int(row['id'].values[0]),
                float(row['area'].values[0]),
                float(row['elevation'].values[0])
            )
            for prop in properties:
                if isinstance(row[prop].values[0], str):
                    self.settings.add_hydro_unit_property_str(
                        prop,
                        row[prop].values[0]
                    )
                else:
                    unit = self._get_unit(row[prop])
                    self.settings.add_hydro_unit_property_double(
                        prop,
                        float(row[prop].values[0]),
                        unit
                    )
            for cover_type, cover_name in zip(self.land_cover_types,
                                              self.land_cover_names):
                fraction = float(row[self.prefix_fraction + cover_name].values[0])
                if np.isnan(fraction):
                    fraction = 0.0
                assert 0 <= fraction <= 1
                self.settings.add_land_cover(cover_name, cover_type, fraction)

    def set_connectivity(self, connectivity: pd.DataFrame | Path | str):
        """
        Set the connectivity of the hydro units.

        Parameters
        ----------
        connectivity
            File or Dataframe containing the connectivity information as generated by
            catchment.calculate_connectivity().
        """
        if isinstance(connectivity, (str, Path)):
            connectivity = pd.read_csv(connectivity, header=0, skiprows=[1])

        if not isinstance(connectivity, pd.DataFrame):
            raise TypeError('The connectivity must be a pandas DataFrame.')

        if not all(col in connectivity.columns for col in ['id', 'connectivity']):
            raise ValueError('The connectivity DataFrame must contain "id" and '
                             '"connectivity" columns.')

        # Loop through the rows and set the connectivity to the basin settings
        for _, row in connectivity.iterrows():
            if isinstance(row['id'], pd.Series):
                hydro_unit_id = int(row['id'].iloc[0])
            else:
                hydro_unit_id = int(row['id'])

            if isinstance(row['connectivity'], pd.Series):
                connectivity_val = row[('connectivity', '-')]
            else:
                connectivity_val = row['connectivity']

            if isinstance(connectivity_val, str):
                connectivity_val = ast.literal_eval(connectivity_val)

            if not connectivity_val:
                continue

            # Extract connected unit IDs and their ratios
            connected_units = {int(k): float(v) for k, v in connectivity_val.items()}

            # Check that the sum of ratios is 1
            total_ratio = sum(connected_units.values())
            if not np.isclose(total_ratio, 1.0):
                raise ValueError(f'The sum of connectivity ratios for hydro unit '
                                 f'{hydro_unit_id} is {total_ratio}, '
                                 'which is not equal to 1.')

            for connected_unit_id, ratio in connected_units.items():
                if connected_unit_id == hydro_unit_id:
                    raise ValueError(f'Hydro unit {hydro_unit_id} cannot '
                                     f'be connected to itself.')
                if ratio < 0:
                    raise ValueError(f'Negative connectivity ratio {ratio} for '
                                     f'hydro unit {hydro_unit_id} '
                                     f'and connected unit {connected_unit_id}.')
                elif ratio > 1:
                    raise ValueError(f'Connectivity ratio {ratio} for hydro unit '
                                     f'{hydro_unit_id} and connected unit '
                                     f'{connected_unit_id} cannot be greater than 1.')

                if hydro_unit_id not in self.hydro_units[('id', '-')].values:
                    raise ValueError(f'Hydro unit {hydro_unit_id} not '
                                     f'found in hydro units.')
                if connected_unit_id not in self.hydro_units[('id', '-')].values:
                    raise ValueError(f'Connected hydro unit {connected_unit_id} '
                                     f'not found in hydro units.')

                # Add the connectivity to the settings
                self.settings.add_lateral_connection(
                    hydro_unit_id, connected_unit_id, ratio)

    @staticmethod
    def _check_column_names(file_content: pd.DataFrame):
        # Rename unnamed columns to 'No Unit'
        new_column_names = []
        for i, col in enumerate(file_content.columns):
            name = col[0]
            if name is None or name in ['-', '', ' '] or 'Unnamed' in name:
                name = f'{i}'
            unit = str(get_unit_enum(col[1]))
            if unit == 'no_unit':
                unit = '-'
            new_column_names.append((name, unit))

        file_content.columns = pd.MultiIndex.from_tuples(new_column_names)

    @staticmethod
    def _get_column_values_unit(
            column_name: str,
            df: pd.DataFrame
    ) -> tuple[np.ndarray, str]:
        col = df.loc[:, column_name]
        unit = col.columns.values[0]
        return col.values.flatten(), unit

    @staticmethod
    def _get_unit(prop: pd.Series) -> str:
        return str(prop.index[0])

    def _check_land_cover_areas_match(self, columns_areas: list[str]):
        if len(columns_areas) != len(self.land_cover_names):
            raise ValueError('The length of the provided "columns_areas" do not match '
                             'the size ot the land cover names.')
        for col in columns_areas:
            if col not in self.land_cover_names:
                raise ValueError(f'The land cover "{col}" was not found in the '
                                 f'defined land covers.')

    def _compute_area_portions(
            self,
            area_values: np.ndarray,
            area_unit: str
    ):
        # Compute total area
        area = np.sum(area_values, axis=1)

        # Normalize land covers
        fractions = area_values / area[:, None]

        # Insert the results in the dataframe
        self.hydro_units[('area', area_unit)] = area
        for idx, cover_name in enumerate(self.land_cover_names):
            field_name = self.prefix_fraction + cover_name
            self.hydro_units[(field_name, 'fraction')] = fractions[:, idx]

    def _set_units_data(self, data: pd.DataFrame):
        assert isinstance(data, pd.DataFrame)
        assert 'area' in data.columns
        assert 'elevation' in data.columns

        if 'id' not in data.columns:
            data['id'] = range(1, 1 + len(data))

        self.hydro_units = data
        idx = self.prefix_fraction + 'ground'
        self.hydro_units[idx] = np.ones(len(self.hydro_units['area']))
        self.populate_bounded_instance()

    @staticmethod
    def _check_land_cover_definitions(
            land_cover_types: list[str],
            land_cover_names: list[str]
    ):
        if land_cover_types is None and land_cover_names is None:
            return

        if land_cover_types is None or land_cover_names is None:
            raise ValueError('The land cover name or type is undefined.')

        if len(land_cover_types) != len(land_cover_names):
            raise ValueError('The length of the land cover types & names do not match.')
