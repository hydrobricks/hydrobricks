from datetime import datetime

import pandas as pd
from hydrobricks import utils


class TimeSeries:
    """Class for generic time series data"""

    def __init__(self):
        self.date = []
        self.data_raw = []
        self.data_spatialized = []
        self.data_name = []

    def load_from_csv(self, path, column_date, date_format, content):
        """
        Read time series data from csv file.

        Parameters
        ----------
        path : str
            Path to the csv file containing hydro units data.
        column_date : str
            Column name containing the date.
        date_format : str
            Format of the date
        content : dict
            Type of data and column name containing the data.
            Example: {'precipitation': 'Precipitation (mm)'}
        """
        file_content = pd.read_csv(
            path, parse_dates=[column_date],
            date_parser=lambda x: datetime.strptime(x, date_format))

        self.date = file_content[column_date].to_numpy()

        for col in content:
            self.data_name.append(col)
            self.data_raw.append(file_content[content[col]].to_numpy())

    def _date_as_mjd(self):
        return utils.date_as_mjd(self.date)
