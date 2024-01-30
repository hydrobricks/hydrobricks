import numpy as np
import pandas as pd

import hydrobricks as hb

from .time_series import TimeSeries1D


class Observations(TimeSeries1D):
    """Class for forcing data"""

    def __init__(self):
        super().__init__()

    def compute_reference_metric(self, metric, n_evals=100):
        """
        Compute a reference for the provided metric (goodness of fit)
        by block bootstrapping the observed series n_evals times (100 times by default),
        evaluating the bootstrapped series using the provided metric and computing
        the mean of the results.

        Parameters
        ----------
        metric : str
            The abbreviation of the function as defined in HydroErr
            (https://hydroerr.readthedocs.io/en/stable/list_of_metrics.html)
            Examples: nse, kge_2012, ...
        n_evals : int
            Number of evaluations to perform (default: 100).

        Returns
        -------
        The mean value of n_evals realization of the selected metric.
        """
        df = self.time.to_frame(name='date').copy()
        df['data'] = self.data[0]
        df['year'] = pd.DatetimeIndex(df['date']).year
        df = df.set_index('date')
        # Get rid of February 29 to always have years of 365 days
        df = df[df.index.strftime('%m-%d') != '02-29']
        years = df.year.unique()

        df = df.set_index('year')

        metrics = np.empty(n_evals)
        for i in range(n_evals):
            sampled_years = np.random.choice(years, size=years.size, replace=True)
            new_df = df.loc[sampled_years].copy()
            value = hb.evaluate(new_df.data.values, df.data.values, metric)
            metrics[i] = value

        ref_metric = np.mean(metrics)

        return ref_metric
