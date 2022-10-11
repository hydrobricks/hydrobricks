from .time_series import TimeSeries


class Observations(TimeSeries):
    """Class for forcing data"""

    def __init__(self):
        super().__init__()
