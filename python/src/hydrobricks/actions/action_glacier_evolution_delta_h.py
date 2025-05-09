from pathlib import Path
import pandas as pd

import _hydrobricks as _hb
import hydrobricks as hb

from .action import Action
from ..preprocessing import GlacierEvolutionDeltaH


class ActionGlacierEvolutionDeltaH(Action):
    """
    Class for the glacier evolution based on the delta-h method.

    The glacier evolution is based on the delta-h method, which spatializes the glacier
    melt using a lookup table. The lookup table in computed by the routine
    preprocessing/glacier_evolution_delta_h.py.
    """

    def __init__(self):
        super().__init__()
        self.action = _hb.ActionGlacierEvolutionDeltaH()

    def load_from_csv(self, dir_path, land_cover='glacier',
                      filename_area='glacier_evolution_lookup_table_area.csv',
                      filename_volume='glacier_evolution_lookup_table_volume.csv',
                      update_month='October'):
        """
        Read the glacier evolution lookup table from a csv file. The file should
        contain the glacier area evolution (in m2) for each hydro unit and increment
        (typically 100). The first column should contain the increments, starting from
        0 for the initial condition to 100 for the complete glacier disappearance.
        The first row should contain the hydro unit ids for the units containing
        glaciers.

        Parameters
        ----------
        dir_path : str|Path
            Path to the directory containing the lookup table.
        land_cover : str
            The land cover name to apply the changes. Default is 'glacier'.
        filename_area : str
            Name of the lookup table file for the glacier area. Default is
            'glacier_evolution_lookup_table_area.csv'.
        filename_volume : str
            Name of the lookup table file for the glacier volume. Default is
            'glacier_evolution_lookup_table_volume.csv'.
        update_month : str|int
            The month to apply the changes. Full english name or number (1-12).
            The update will be applied at the beginning of the month, every year.
            Default is 'October'.

        Example of a file (with areas in m2)
        -----------------
        	2	    3	    4	    5	    6
        0	2500.0	48125.0	34375.0	54375.0	65000.0
        1	2125.2	44069.6	31544.8	51044.3	61726.6
        2	1668.2	39600.1	28428.8	47478.1	58268.9
        3	1024.3	34555.2	24914.0	43617.1	54591.8
        4	0	    28628.9	20776.8	39371.4	50646.9
        5	0	    21024.9	15298.5	34564.7	46342.8
        6	0	    7906.0	7475.2	28921.9	41597.3
        7	0	    0	    1231.4	21121.3	36034.0
        8	0	    0	    0	    10242.1	28498.8
        """
        if isinstance(dir_path, str):
            dir_path = Path(dir_path)
        full_path_area = dir_path / filename_area
        full_path_volume = dir_path / filename_volume

        lookup_table_area = pd.read_csv(full_path_area)
        lookup_table_volume = pd.read_csv(full_path_volume)

        self._populate_bounded_instance(lookup_table_area, lookup_table_volume,
                                        land_cover, update_month)

    def get_from_object(self, obj: GlacierEvolutionDeltaH, land_cover='glacier',
                        update_month='October'):
        """
        Get the glacier evolution lookup table from the GlacierEvolutionDeltaH
        instance.

        Parameters
        ----------
        obj : GlacierEvolutionDeltaH
            The GlacierEvolutionDeltaH instance.
        land_cover : str
            The land cover name to apply the changes. Default is 'glacier'.
        update_month : str|int
            The month to apply the changes. Full english name or number (1-12).
            The update will be applied at the beginning of the month, every year.
            Default is 'October'.
        """
        if not isinstance(obj, GlacierEvolutionDeltaH):
            raise ValueError("The object is not a GlacierEvolutionDeltaH instance.")

        lookup_table_area = obj.get_lookup_table_area()
        lookup_table_volume = obj.get_lookup_table_volume()

        self._populate_bounded_instance(lookup_table_area, lookup_table_volume,
                                        land_cover, update_month)

    def _populate_bounded_instance(self, lookup_table_area, lookup_table_volume,
                                   land_cover, update_month):

        # Convert the month name to a number
        if isinstance(update_month, int):
            if update_month < 1 or update_month > 12:
                raise ValueError("Month number must be between 1 and 12.")
            month_num = update_month
        elif isinstance(update_month, str):
            month_mapping = {
                'January': 1, 'February': 2, 'March': 3, 'April': 4,
                'May': 5, 'June': 6, 'July': 7, 'August': 8,
                'September': 9, 'October': 10, 'November': 11, 'December': 12
            }
            month_num = month_mapping.get(update_month, None)
            if month_num is None:
                raise ValueError(f"Invalid month name: {update_month}")
        else:
            raise ValueError("Month must be a string or an integer.")

        # Get the hydro unit ids from the first row
        hu_ids = lookup_table_area.columns[1:].astype(int).values
        assert lookup_table_volume.columns[1:].astype(int).values == hu_ids, \
            "The hydro unit ids in the area and volume tables do not match."

        # Get the increments from the first column
        increments = lookup_table_area.iloc[:, 0].astype(float).values
        assert lookup_table_volume.iloc[:, 0].astype(float).values == increments, \
            "The increments in the area and volume tables do not match."

        # Get the areas from the rest of the table
        areas = lookup_table_area.iloc[:, 1:].astype(float).values

        # Get the volumes from the rest of the table
        volumes = lookup_table_volume.iloc[:, 1:].astype(float).values

        self.action.add_lookup_tables(month_num, land_cover, hu_ids, increments,
                                      areas, volumes)
