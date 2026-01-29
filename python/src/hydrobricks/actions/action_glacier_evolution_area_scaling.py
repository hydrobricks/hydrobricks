from __future__ import annotations

from pathlib import Path

import numpy as np
import pandas as pd

from hydrobricks._hydrobricks import (
    ActionGlacierEvolutionAreaScaling as _ActionGlacierEvolutionAreaScaling,
)
from hydrobricks._exceptions import ConfigurationError
from hydrobricks.actions import Action
from hydrobricks.preprocessing.glacier_evolution_area_scaling import (
    GlacierEvolutionAreaScaling,
)


class ActionGlacierEvolutionAreaScaling(Action):
    """
    Class for the glacier evolution based on a simple area-volume scaling.

    The glacier evolution is based on a simple area-volume scaling, which computes the
    glacier area evolution based on the glacier volume evolution for each hydro unit.
    The lookup table is computed by the routine
    preprocessing/glacier_evolution_area_scaling.py
    """

    def __init__(self) -> None:
        """
        Initialize ActionGlacierEvolutionAreaScaling instance.

        This action manages glacier evolution using area-volume scaling relationships,
        updating glacier areas based on volume changes for each hydro unit.
        """
        super().__init__()
        self.name: str = "ActionGlacierEvolutionAreaScaling"
        self.action: _ActionGlacierEvolutionAreaScaling = _ActionGlacierEvolutionAreaScaling()

    def load_from_csv(
            self,
            dir_path: str | Path,
            land_cover: str = 'glacier',
            filename_area: str = 'glacier_evolution_lookup_table_area.csv',
            filename_volume: str = 'glacier_evolution_lookup_table_volume.csv',
            update_month: str | int = 'October'
    ) -> None:
        """
        Read the glacier evolution lookup table from CSV files.

        The files should contain the glacier area and volume evolution (in m²) for each
        hydro unit and increment (typically 100). The first row should contain the hydro
        unit IDs for the units containing glaciers. There should be no index column in
        the file; the first column should contain data.

        Parameters
        ----------
        dir_path
            Path to the directory containing the lookup table files.
        land_cover
            The land cover name to apply the changes. Default: 'glacier'
        filename_area
            Name of the lookup table file for the glacier area.
            Default: 'glacier_evolution_lookup_table_area.csv'
        filename_volume
            Name of the lookup table file for the glacier volume.
            Default: 'glacier_evolution_lookup_table_volume.csv'
        update_month
            The month to apply the changes. Full English name or number (1-12).
            The update will be applied at the beginning of the month, every year.
            Default: 'October'

        Raises
        ------
        FileNotFoundError
            If the specified CSV files do not exist.
        ValueError
            If the CSV format is incorrect or data is inconsistent.
        AssertionError
            If hydro unit IDs or table shapes don't match between area and volume files.

        Examples
        --------
        >>> action = ActionGlacierEvolutionAreaScaling()
        >>> action.load_from_csv('./data', land_cover='glacier', update_month='October')

        Example of a file (with areas in m²)
        -------------------------------------
        2       3       4       5       6
        2500.0  48125.0 34375.0 54375.0 65000.0
        2125.2  44069.6 31544.8 51044.3 61726.6
        1668.2  39600.1 28428.8 47478.1 58268.9
        1024.3  34555.2 24914.0 43617.1 54591.8
        0       28628.9 20776.8 39371.4 50646.9
        0       21024.9 15298.5 34564.7 46342.8
        0       7906.0  7475.2  28921.9 41597.3
        0       0       1231.4  21121.3 36034.0
        0       0       0       10242.1 28498.8
        """
        if isinstance(dir_path, str):
            dir_path = Path(dir_path)
        full_path_area = dir_path / filename_area
        full_path_volume = dir_path / filename_volume

        lookup_table_area = pd.read_csv(full_path_area)
        lookup_table_volume = pd.read_csv(full_path_volume)

        self._populate_bounded_instance(
            lookup_table_area,
            lookup_table_volume,
            land_cover,
            update_month
        )

    def load_from(
            self,
            obj: GlacierEvolutionAreaScaling,
            land_cover: str = 'glacier',
            update_month: str | int = 'October'
    ) -> None:
        """
        Get the glacier evolution lookup table from a GlacierEvolutionAreaScaling instance.

        Parameters
        ----------
        obj
            The GlacierEvolutionAreaScaling instance containing lookup tables.
        land_cover
            The land cover name to apply the changes. Default: 'glacier'
        update_month
            The month to apply the changes. Full English name or number (1-12).
            The update will be applied at the beginning of the month, every year.
            Default: 'October'

        Raises
        ------
        ConfigurationError
            If the object is not a GlacierEvolutionAreaScaling instance.
        """
        if not isinstance(obj, GlacierEvolutionAreaScaling):
            raise ConfigurationError(
                "The object is not a GlacierEvolutionAreaScaling instance.",
                item_value=type(obj).__name__,
                reason='Invalid object type'
            )

        lookup_table_area = obj.get_lookup_table_area()
        lookup_table_volume = obj.get_lookup_table_volume()

        self._populate_bounded_instance(
            lookup_table_area,
            lookup_table_volume,
            land_cover,
            update_month
        )

    def get_month(self) -> int:
        """
        Get the month to apply the changes.

        Returns
        -------
        int
            The month number (1-12) when changes are applied.
        """
        return self.action.get_month()

    def get_land_cover_name(self) -> str:
        """
        Get the land cover name (glacier name) to apply the changes.

        Returns
        -------
        str
            The land cover name (glacier name) to apply the changes.
        """
        return self.action.get_land_cover_name()

    def get_hydro_unit_ids(self) -> np.ndarray:
        """
        Get the lookup table hydro unit IDs.

        Returns
        -------
        np.ndarray
            Array of hydro unit IDs (integers) for which glacier evolution is tracked.
        """
        return self.action.get_hydro_unit_ids()

    def get_lookup_table_area(self) -> np.ndarray:
        """
        Get the lookup table areas.

        Returns
        -------
        np.ndarray
            2D array of glacier areas (in m²) for each hydro unit and time increment.
            Shape: (n_increments, n_hydro_units)
        """
        return self.action.get_lookup_table_area()

    def get_lookup_table_volume(self) -> np.ndarray:
        """
        Get the lookup table volumes.

        Returns
        -------
        np.ndarray
            2D array of glacier volumes (in m³) for each hydro unit and time increment.
            Shape: (n_increments, n_hydro_units)
        """
        return self.action.get_lookup_table_volume()

    def _populate_bounded_instance(
            self,
            lookup_table_area: pd.DataFrame,
            lookup_table_volume: pd.DataFrame,
            land_cover: str,
            update_month: str | int
    ) -> None:
        """
        Populate the internal C++ instance with lookup table data.

        Parameters
        ----------
        lookup_table_area
            DataFrame containing area evolution data with hydro unit IDs as columns.
        lookup_table_volume
            DataFrame containing volume evolution data with hydro unit IDs as columns.
        land_cover
            The land cover name to apply changes to.
        update_month
            The month (name or number) to apply changes.

        Raises
        ------
        AssertionError
            If hydro unit IDs or table shapes don't match between area and volume.
        """
        month_num = self._convert_month_to_number(update_month)

        # Get the hydro unit ids from the first row
        hu_ids = lookup_table_area.columns.astype(int).values
        hu_ids_volume = lookup_table_volume.columns.astype(int).values
        assert np.array_equal(hu_ids, hu_ids_volume), \
            "The hydro unit ids in the area and volume tables do not match."

        # Get the areas from the rest of the table
        areas = lookup_table_area.astype(float).values

        # Get the volumes from the rest of the table
        volumes = lookup_table_volume.astype(float).values

        assert areas.shape == volumes.shape, \
            "The areas and volumes tables do not have the same shape."

        self.action.add_lookup_tables(month_num, land_cover, hu_ids, areas, volumes)

        self.is_initialized = True
