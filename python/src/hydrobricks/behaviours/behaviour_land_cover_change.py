import pandas as pd

import _hydrobricks as _hb
from hydrobricks import utils

from .behaviour import Behaviour


class BehaviourLandCoverChange(Behaviour):
    """Class for the land cover changes."""

    def __init__(self):
        super().__init__()
        self.behaviour = _hb.BehaviourLandCoverChange()

    def load_from_csv(self, path, hydro_units, area_unit, match_with='elevation'):
        """
        Read hydro units properties from csv file. The first column of the file must
        contain the information to identify the hydro unit id, such as the id or the
        elevation (when using elevation bands). The next columns must contain the
        changes at different dates for each hydro unit. The first line must contain
        the name of the land cover to change. The second line must contain the date
        of the change in a format easily parsed by Python.

        Parameters
        ----------
        path : str|Path
            Path to the csv file containing hydro units data.
        hydro_units : HydroUnits
            The hydro units to match the land cover changes against.
        area_unit: str
            Unit for the area values: "m2" or "km2"
        match_with : str
            Information used to identify the hydro units. Options: 'elevation', 'id'

        Example of a file (with areas in km2)
        -----------------
        elevation   glacier      glacier      glacier      glacier      glacier
                    01/08/2020   01/08/2025   01/08/2030   01/08/2035   01/08/2040
        4274        0.013        0.003        0            0            0
        4310        0.019        0.009        0            0            0
        4346        0.052        0.042        0.032        0.022        0.012
        4382        0.072        0.062        0.052        0.042        0.032
        4418        0.129        0.119        0.109        0.099        0.089
        4454        0.252        0.242        0.232        0.222        0.212
        4490        0.288        0.278        0.268        0.258        0.248
        4526        0.341        0.331        0.321        0.311        0.301
        4562        0.613        0.603        0.593        0.583        0.573
        """
        file_content = pd.read_csv(path, header=None)
        file_content.insert(loc=0, column='hydro_unit', value=0)

        self._match_hydro_unit_ids(file_content, hydro_units, match_with)
        self._extract_changes(area_unit, file_content)

    def get_changes_nb(self):
        """
        Get the number of changes registered.
        """
        return self.behaviour.get_changes_nb()

    def get_land_covers_nb(self):
        """
        Get the number of land covers registered.
        """
        return self.behaviour.get_land_covers_nb()

    @staticmethod
    def _match_hydro_unit_ids(file_content, hydro_units, match_with):
        hu_df = hydro_units.hydro_units
        for row, change in file_content.iterrows():
            if row < 2:
                continue
            if match_with == 'elevation':
                idx_id = hu_df.index[hu_df.elevation == int(change[0])].to_list()[0]
            elif match_with == 'id':
                idx_id = int(change[0])
            else:
                raise ValueError(f'No option "{match_with}" for "match_with".')
            file_content.loc[row, 'hydro_unit'] = hu_df.loc[idx_id, 'id']

    def _extract_changes(self, area_unit, file_content):
        for col in list(file_content):
            if col == 'hydro_unit' or col == 0:
                continue
            land_cover = file_content.loc[0, col]
            date = pd.Timestamp(file_content.loc[1, col])
            mjd = utils.date_as_mjd(date)

            for row in range(2, len(file_content[col])):
                hu_id = file_content.loc[row, 'hydro_unit']
                area = float(file_content.loc[row, col])
                area = utils.area_in_m2(area, area_unit)

                self.behaviour.add_change(mjd, hu_id, land_cover, area)
