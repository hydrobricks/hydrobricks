import importlib
import numpy as np
import pandas as pd

from .time_series import TimeSeries1D


class Observations(TimeSeries1D):
    """Class for forcing data"""

    def __init__(self):
        super().__init__()

    @staticmethod
    def _eval_metric(metric, simulations, observations):
        """
        Evaluate the simulation using the provided metric (goodness of fit).

        Parameters
        ----------
        metric : str
            The abbreviation of the function as defined in HydroErr
            (https://hydroerr.readthedocs.io/en/stable/list_of_metrics.html)
            Examples: nse, kge_2012, ...
        simulations
            The time series of the simulations with dates matching the observed
            series.
        observations
            The time series of the observations with dates matching the simulated
            series.

        Returns
        -------
        The value of the selected metric.
        """
        eval_fct = getattr(importlib.import_module('HydroErr'), metric)

        return eval_fct(simulations, observations)

    def compute_reference_metric(self, metric):
        """
        Compute a reference for the provided metric (goodness of fit)
        by block bootstrapping the observed series 100 times, evaluating
        the bootstrapped series using the provided metric and computing
        the mean of the results.

        Parameters
        ----------
        metric: str
            The abbreviation of the function as defined in HydroErr
            (https://hydroerr.readthedocs.io/en/stable/list_of_metrics.html)
            Examples: nse, kge_2012, ...

        Returns
        -------
        The mean value of 100 realization of the selected metric.
        """
        df = self.time.to_frame().copy()
        df['data'] = self.data[0]
        df['year'] = pd.DatetimeIndex(df['date']).year
        df = df.set_index('date')
        # Get rid of the 29 February to always have years of 365 days
        df = df[df.index.strftime('%m-%d') != '02-29']
        years = df.year.unique()

        df = df.set_index('Year')

        metrics = np.empty(100)
        for i in range(100):
            sampled_years = np.random.choice(years, size=years.size, replace=True)
            new_df = df.loc[sampled_years].copy()
            value = self._eval_metric(metric, new_df.data.values, df.data.values)
            metrics[i] = value

        ref_metric = np.mean(metrics)

        return ref_metric
