import ast
from pathlib import Path

import numpy as np
import pandas as pd

from hydrobricks import Dataset
from hydrobricks._hydrobricks import SettingsBasin
from hydrobricks._optional import HAS_NETCDF
from hydrobricks._exceptions import DataError, ConfigurationError, DependencyError
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
        DataFrame containing the hydro units data.

    Attributes
    ----------
    land_cover_types : list[str]
        List of land cover types. Default: ['ground']
    land_cover_names : list[str]
        List of land cover names. Default: ['ground']
    hydro_units : pd.DataFrame
        Dataframe containing the hydro units data.
    """

    def __init__(
            self,
            land_cover_types: list[str] | None = None,
            land_cover_names: list[str] | None = None,
            data: pd.DataFrame | None = None
    ) -> None:
        """
        Initialize HydroUnits instance.

        Parameters
        ----------
        land_cover_types
            List of land cover type identifiers. If None, defaults to ['ground'].
        land_cover_names
            List of land cover display names. If None, defaults to ['ground'].
        data
            Pre-existing DataFrame with hydro units data. If None, creates empty DataFrame.

        Raises
        ------
        ValueError
            If land_cover_types and land_cover_names have different lengths.
        """
        self.settings = SettingsBasin()
        self._check_land_cover_definitions(land_cover_types, land_cover_names)
        if not land_cover_types:
            land_cover_types = ['ground']
        if not land_cover_names:
            land_cover_names = ['ground']
        self.land_cover_types: list[str] = land_cover_types
        self.land_cover_names: list[str] = land_cover_names
        self.prefix_fraction: str = 'fraction-'
        land_cover_cols: list[tuple[str, str]] = []
        for item in land_cover_names:
            land_cover_cols.append((f'{self.prefix_fraction}{item}', 'fraction'))
        if data is not None:
            self._set_units_data(data)
        else:
            self.hydro_units: pd.DataFrame = pd.DataFrame(columns=land_cover_cols)

    def load_from_csv(
            self,
            path: str | Path,
            column_elevation: str | None = None,
            column_area: str | None = None,
            column_fractions: dict[str, str] | None = None,
            columns_areas: dict[str, str] | None = None,
            other_columns: dict[str, str] | None = None
    ) -> None:
        """
        Read hydro units properties from CSV file. The file must contain two header
        rows. The first row contains the column names and the second row contains the
        units. The file must contain at minimum the units area.

        Parameters
        ----------
        path
            Path to the CSV file containing hydro units data.
        column_elevation
            Column name containing the elevation values.
            If None, looks for 'elevation' column. Default: None
        column_area
            Column name containing the total area values.
            If None, looks for 'area' column. Default: None
        column_fractions
            NOT IMPLEMENTED. Dictionary mapping land cover names to area fraction column names.
            Default: None
        columns_areas
            Dictionary mapping land cover names to area column names.
            Cannot be used with column_area. Default: None
        other_columns
            Dictionary mapping property names to column names in the CSV file.
            Example: {'slope': 'Slope', 'aspect': 'Aspect'}
            Default: None

        Raises
        ------
        FileNotFoundError
            If the CSV file does not exist.
        ValueError
            If required columns are missing or are inconsistent.
        NotImplementedError
            If column_fractions is provided (not yet implemented).
        """
        file_content = pd.read_csv(path, header=[0, 1])
        self._check_column_names(file_content)

        # Check that the id column is present
        if 'id' not in file_content.columns:
            raise DataError(
                'The "id" column is required in the file.',
                data_type='hydro units',
                reason='Missing required column'
            )
        vals, _ = self._get_column_values_unit('id', file_content)
        self.add_property(('id', '-'), vals, set_first=True)

        if column_elevation is not None:
            vals, unit = self._get_column_values_unit(column_elevation, file_content)
            self.add_property(('elevation', unit), vals)
        elif 'elevation' in file_content.columns:
            vals, unit = self._get_column_values_unit('elevation', file_content)
            self.add_property(('elevation', unit), vals)
        else:
            raise DataError(
                'The "elevation" column is required in the file.',
                data_type='hydro units',
                reason='Missing required column'
            )

        if column_area is not None and columns_areas is not None:
            raise DataError(
                'The "column_area" and "columns_areas" cannot be '
                'provided at the same time.',
                data_type='hydro units',
                reason='Ambiguous column specification'
            )

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
                    raise DataError(
                        'The area units do not match.',
                        data_type='hydro units',
                        reason='Inconsistent units across land covers'
                    )
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
            raise ConfigurationError(
                'The "column_fractions" parameter is not yet implemented.'
            )

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

    def save_to_csv(self, path: str | Path) -> None:
        """
        Save the hydro units to a csv file.

        Exports the hydro units DataFrame to a CSV file with multi-level header
        containing both property names and units.

        Parameters
        ----------
        path
            Path to the output file.

        Raises
        ------
        ValueError
            If no hydro units data is available.
        """
        if self.hydro_units is None:
            raise DataError(
                "No hydro units to save.",
                data_type='hydro units',
                reason='Empty hydro units'
            )

        # Save to csv file with units in the header
        self.hydro_units.to_csv(path, header=True, index=False)

    def save_as(self, path: str) -> None:
        """
        Create a file containing the hydro unit properties. Such a file can be used in
        the command-line version of hydrobricks.

        Saves hydro units and land cover information to a netCDF4 file with proper
        dimensions and variable attributes.

        Parameters
        ----------
        path
            Path of the file to create.

        Raises
        ------
        ImportError
            If netcdf4 is not installed.
        """
        if not HAS_NETCDF:
            raise DependencyError(
                'netcdf4 is required to save hydro units to file.',
                package_name='netcdf4',
                operation='HydroUnits.save_as',
                install_command='pip install netcdf4'
            )

        # Create netCDF file
        nc = Dataset(path, 'w', 'NETCDF4')

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
            The property name to check. Should match a column name in the hydro_units DataFrame.

        Returns
        -------
        bool
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
    ) -> None:
        """
        Add a property to the hydro units.

        Adds a new column to the hydro_units DataFrame with the specified property
        name, unit, and values. Can optionally insert as the first column.

        Parameters
        ----------
        column_tuple
            Tuple containing (property_name, unit_string). Example: ('elevation', 'm')
        values
            Numpy array containing the property values for each hydro unit.
        set_first
            If True, the property is added as the first column. Default: False

        Raises
        ------
        ValueError
            If values length doesn't match the number of hydro units.
        """
        df = pd.DataFrame(values, columns=pd.MultiIndex.from_tuples(
            [column_tuple], names=['Property', 'Unit']))

        if self.hydro_units is None:
            self.hydro_units = df
        else:
            if set_first:
                self.hydro_units = pd.concat([df, self.hydro_units], axis=1)
            else:
                self.hydro_units = pd.concat([self.hydro_units, df], axis=1)

    def get_hydro_unit_count(self) -> int:
        """
        Get the number of hydro units.

        Returns
        -------
        Number of hydro units.
        """
        return len(self.hydro_units)

    def check_land_cover_fractions_not_empty(self) -> None:
        """
        Check that the land cover fractions are not empty.

        Validates that all land cover fractions have been defined. If there is a single
        land cover type (e.g. 'ground'), automatically sets it to 1.0 for all hydro units.

        Raises
        ------
        ValueError
            If any land cover fraction contains NaN values (when multiple land cover types exist).
        """
        if len(self.land_cover_names) == 1:
            self.hydro_units[self.prefix_fraction + self.land_cover_names[0]] = 1.0
            return

        for cover_name in self.land_cover_names:
            field_name = self.prefix_fraction + cover_name
            if self.hydro_units[field_name].isnull().values.any():
                raise DataError(
                    f'The land cover "{cover_name}" contains NaN values.',
                    data_type='land cover',
                    reason='Invalid data with NaN values'
                )

    def initialize_land_cover_fractions(self) -> None:
        """
        Initialize land cover fractions with default values.

        Sets the land cover fractions of 'ground' to 1.0 and all other land cover types to 0.0
        for all hydro units. Used as a starting point before applying specific land cover changes.
        """
        # Set the land cover fractions of 'ground' to 1 and the rest to 0
        for cover_name in self.land_cover_names:
            field_name = self.prefix_fraction + cover_name
            self.hydro_units[(field_name, 'fraction')] = 0.0
        self.hydro_units[(self.prefix_fraction + 'ground', 'fraction')] = 1.0

    def initialize_from_land_cover_change(
            self,
            land_cover_name: str,
            land_cover_change: pd.DataFrame
    ) -> None:
        """
        Initialize the hydro units from the first values of a land cover change dataframe.

        Updates the land cover fractions for specified hydro units based on a land cover change
        dataframe. Automatically adjusts the 'ground' land cover fraction to maintain conservation.

        Parameters
        ----------
        land_cover_name
            The name of the land cover to initialize.
        land_cover_change
            The land cover change dataframe with columns 'hydro_unit' and area values.

        Raises
        ------
        ValueError
            If computed land cover fraction is not in the range [0, 1].
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
            if not 0 <= fraction <= 1:
                raise DataError(
                    f'Land cover fraction {fraction} for unit {unit_id} is outside [0, 1].',
                    data_type='land cover fraction',
                    reason='Fraction outside valid range'
                )

            # Set the land cover fraction
            self.hydro_units.loc[hu_idx, (field_name, 'fraction')] = fraction
            self.hydro_units.loc[hu_idx, (ground_name, 'fraction')] -= fraction

        self.populate_bounded_instance()

    def populate_bounded_instance(self) -> None:
        """
        Populate the SettingsBasin instance from current hydro units data.

        Updates the internal SettingsBasin object with current hydro unit properties,
        sorted by elevation in descending order. Includes land cover fractions and
        all custom properties.
        """
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

    def set_connectivity(self, connectivity: pd.DataFrame | Path | str) -> None:
        """
        Set the connectivity of the hydro units.

        Configures how water flows between hydro units by setting lateral connections
        with specified ratios. Can accept connectivity data as a DataFrame or load from file.

        Parameters
        ----------
        connectivity
            File or Dataframe containing the connectivity information as generated by
            catchment.calculate_connectivity().

        Raises
        ------
        TypeError
            If connectivity is not a DataFrame or valid file path.
        ValueError
            If connectivity DataFrame is missing required columns or has invalid values.
        """
        if isinstance(connectivity, (str, Path)):
            connectivity = pd.read_csv(connectivity, header=0, skiprows=[1])

        if not isinstance(connectivity, pd.DataFrame):
            raise DataError(
                'The connectivity must be a pandas DataFrame.',
                data_type='connectivity',
                reason='Invalid data type'
            )

        if not all(col in connectivity.columns for col in ['id', 'connectivity']):
            raise DataError(
                'The connectivity DataFrame must contain "id" '
                'and "connectivity" columns.',
                data_type='connectivity',
                reason='Missing required columns'
            )

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
                raise DataError(
                    f'The sum of connectivity ratios for hydro unit {hydro_unit_id} '
                    f'is {total_ratio}, which is not equal to 1.',
                    data_type='connectivity',
                    reason='Invalid connectivity ratios sum'
                )

            for connected_unit_id, ratio in connected_units.items():
                if connected_unit_id == hydro_unit_id:
                    raise DataError(
                        f'Hydro unit {hydro_unit_id} cannot be connected to itself.',
                        data_type='connectivity',
                        reason='Self-connection not allowed'
                    )
                if ratio < 0:
                    raise DataError(
                        f'Negative connectivity ratio {ratio} for hydro unit '
                        f'{hydro_unit_id} and connected unit {connected_unit_id}.',
                        data_type='connectivity',
                        reason='Negative connectivity ratio'
                    )
                elif ratio > 1:
                    raise DataError(
                        f'Connectivity ratio {ratio} for hydro unit '
                        f'{hydro_unit_id} and connected unit '
                        f'{connected_unit_id} cannot be greater than 1.',
                        data_type='connectivity',
                        reason='Ratio exceeds maximum value'
                    )

                if hydro_unit_id not in self.hydro_units[('id', '-')].values:
                    raise DataError(
                        f'Hydro unit {hydro_unit_id} not found in hydro units.',
                        data_type='connectivity',
                        reason='Undefined hydro unit'
                    )
                if connected_unit_id not in self.hydro_units[('id', '-')].values:
                    raise DataError(
                        f'Connected hydro unit {connected_unit_id} '
                        f'not found in hydro units.',
                        data_type='connectivity',
                        reason='Undefined connected hydro unit'
                    )

                # Add the connectivity to the settings
                self.settings.add_lateral_connection(
                    hydro_unit_id, connected_unit_id, ratio)

    @staticmethod
    def _check_column_names(file_content: pd.DataFrame) -> None:
        """
        Validate and rename columns in a DataFrame read from CSV.

        Renames unnamed or invalid columns to numeric indices, and converts unit
        names to standardized enum format.

        Parameters
        ----------
        file_content
            DataFrame with multi-level column index (name, unit).

        Notes
        -----
        Modifies the DataFrame in-place by updating its column index.
        """
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
        """
        Extract column values and unit from a DataFrame with multi-level columns.

        Parameters
        ----------
        column_name
            Name of the column to extract.
        df
            DataFrame with multi-level column index (name, unit).

        Returns
        -------
        tuple[np.ndarray, str]
            Tuple containing:
            - Flattened numpy array of column values
            - Unit string from the column header
        """
        col = df.loc[:, column_name]
        unit = col.columns.values[0]
        return col.values.flatten(), unit

    @staticmethod
    def _get_unit(prop: pd.Series) -> str:
        """
        Extract unit string from a pandas Series with multi-level index.

        Parameters
        ----------
        prop
            Pandas Series with multi-level index where the first level is the unit string.

        Returns
        -------
        str
            The unit string from the series index.
        """
        return str(prop.index[0])

    def _check_land_cover_areas_match(self, columns_areas: dict[str, str]) -> None:
        """
        Validate that provided land cover area columns match defined land cover names.

        Parameters
        ----------
        columns_areas
            Dictionary mapping land cover names to column names in the data.

        Raises
        ------
        DataError
            If the number of columns doesn't match land cover names or if a land cover
            name is not found in the defined land covers.
        """
        if len(columns_areas) != len(self.land_cover_names):
            raise DataError(
                'The length of the provided "columns_areas" does not match '
                f'the number of land covers ({len(self.land_cover_names)}).',
                data_type='land cover',
                reason='Mismatched land cover count'
            )
        for col in columns_areas:
            if col not in self.land_cover_names:
                raise DataError(
                    f'The land cover "{col}" was not found in the '
                    f'defined land covers.',
                    data_type='land cover',
                    reason='Undefined land cover'
                )

    def _compute_area_portions(
            self,
            area_values: np.ndarray,
            area_unit: str
    ) -> None:
        """
        Compute and store land cover area portions for hydro units.

        Calculates the total area for each hydro unit and computes normalized
        land cover fractions. Stores results in the hydro_units DataFrame.

        Parameters
        ----------
        area_values
            2D array of area values per land cover type and hydro unit.
            Shape: (num_units, num_land_covers)
        area_unit
            Unit string for the area values (e.g., 'm2', 'km2').
        """
        # Compute total area
        area = np.sum(area_values, axis=1)

        # Normalize land covers
        fractions = area_values / area[:, None]

        # Insert the results in the dataframe
        self.hydro_units[('area', area_unit)] = area
        for idx, cover_name in enumerate(self.land_cover_names):
            field_name = self.prefix_fraction + cover_name
            self.hydro_units[(field_name, 'fraction')] = fractions[:, idx]

    def _set_units_data(self, data: pd.DataFrame) -> None:
        """
        Set hydro units data from a pre-existing DataFrame.

        Initializes the hydro_units DataFrame, validates required columns,
        and populates land cover fractions and settings.

        Parameters
        ----------
        data
            DataFrame with required columns 'area' and 'elevation'. If 'id' is
            missing, it will be auto-generated.

        Raises
        ------
        AssertionError
            If required columns ('area', 'elevation') are missing.
        """
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
            land_cover_types: list[str] | None,
            land_cover_names: list[str] | None
    ) -> None:
        """
        Validate land cover types and names consistency.

        Ensures that if either land cover types or names are provided, both must be
        provided and they must have matching lengths.

        Parameters
        ----------
        land_cover_types
            List of land cover type identifiers, or None.
        land_cover_names
            List of land cover display names, or None.

        Raises
        ------
        ValueError
            If one is provided without the other, or if they have different lengths.
        """
        if land_cover_types is None and land_cover_names is None:
            return

        if land_cover_name is None and land_cover_type is None:
            raise DataError(
                'The land cover name or type is undefined.',
                data_type='land cover',
                reason='Missing land cover specification'
            )

        if len(land_cover_names) != len(land_cover_types):
            raise DataError(
                'The length of the land cover types & names do not match.',
                data_type='land cover',
                reason='Mismatched types and names'
            )
