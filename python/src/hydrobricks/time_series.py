from datetime import datetime

import pandas as pd
from hydrobricks import utils


class TimeSeries:
    """Class for generic time series data"""

    def __init__(self):
        self.time = []
        self.data_raw = []
        self.data_spatialized = []
        self.data_name = []

    def load_from_csv(self, path, column_time, time_format, content):
        """
        Read time series data from csv file.

        Parameters
        ----------
        path : str|Path
            Path to the csv file containing hydro units data.
        column_time : str
            Column name containing the time.
        time_format : str
            Format of the time
        content : dict
            Type of data and column name containing the data.
            Example: {'precipitation': 'Precipitation (mm)'}
        """
        file_content = pd.read_csv(
            path, parse_dates=[column_time],
            date_parser=lambda x: datetime.strptime(x, time_format))

        self.time = file_content[column_time]

        for col in content:
            self.data_name.append(col)
            self.data_raw.append(file_content[content[col]].to_numpy())
            self.data_spatialized.append(None)

    def _date_as_mjd(self):
        return utils.date_as_mjd(self.time)
