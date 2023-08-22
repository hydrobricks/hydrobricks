from .time_series import TimeSeries1D


class Observations(TimeSeries1D):
    """Class for forcing data"""

    def __init__(self):
        super().__init__()
