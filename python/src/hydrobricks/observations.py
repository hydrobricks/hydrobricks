import itertools

import numpy as np
import pandas as pd

import hydrobricks as hb

from .time_series import TimeSeries1D


class Observations(TimeSeries1D):
    """Class for forcing data"""

    def __init__(self):
        super().__init__()

    def compute_reference_metric(self, metric, start_date=None, end_date=None,
                                 with_exclusion=False, mean_discharge=False,
                                 all_combinations=False, n_evals=100):
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
        start_date : str
            Start date for which the observed data should be compared to the
            mean of all years.
        end_date : str
            End date for which the observed data should be compared to the
            mean of all years.
        with_exclusion : binary
            If True, avoid using the same year's data for the same position in the
            bootstrapped sample, ensuring no self-selection for specific years.
        mean_discharge : binary
            If True, computes the average on the discharge directly rather than on
            the result of the HydroErr function.
        all_combinations : binary
            If True uses all combinations possible for the bootstrapping.
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

        if mean_discharge:
            # Group by day of year and calculate the mean discharge
            # Extract day and month to ignore the year
            df['day_of_year'] = df.index.strftime('%m-%d')
            mean_discharge_df = df.groupby('day_of_year')['data'].mean()
            # Map the mean discharge back to the original DataFrame based on
            # the day of year
            df['mean_discharge'] = df['day_of_year'].map(mean_discharge_df)
            df.to_csv('mean_discharge.csv', columns=['mean_discharge'],
                      float_format='%.12f')
            df.to_csv('data.csv', columns=['data'], float_format='%.12f')

            if start_date and end_date:
                df = df[(df.index >= start_date) & (df.index <= end_date)]

            ref_metric = hb.evaluate(df.mean_discharge.values, df.data.values, metric)
            return ref_metric

        if len(years) == 1:
            print("Not possible to compute the reference metric on one year only.")
            return -1

        df = df.reset_index()
        df = df.set_index('year')

        # Define the range for which we compare
        if start_date and end_date:
            idx = pd.date_range(start_date, end_date)
            comparing_years = np.unique(idx.year)

            comparing_df = df.set_index('date')
            comparing_df = comparing_df[
                (comparing_df.index >= start_date) & (comparing_df.index <= end_date)]
        else:
            comparing_years = years
            comparing_df = df

        if all_combinations:
            metrics = []
            # Generate all possible combinations (order matters, repetitions allowed)
            all_combinations = list(
                itertools.product(years, repeat=len(comparing_years)))

            for sampled_years in all_combinations:
                sampled_years = np.array(sampled_years)  # Convert tuple to array
                if with_exclusion:
                    diff = sampled_years - comparing_years
                    if not np.all(diff):  # Skip if exclusion condition isn't met
                        continue

                new_df = df.loc[sampled_years].copy()
                value = hb.evaluate(new_df.data.values, comparing_df.data.values,
                                    metric)
                metrics.append(value)

        else:
            metrics = np.empty(n_evals)
            i = 0
            while i < n_evals - 1:
                sampled_years = np.random.choice(years, size=len(comparing_years),
                                                 replace=True)
                if with_exclusion:
                    diff = sampled_years - comparing_years
                    if not np.all(diff):  # Skip if exclusion condition isn't met
                        continue
                i += 1
                new_df = df.loc[sampled_years].copy()
                value = hb.evaluate(new_df.data.values, comparing_df.data.values,
                                    metric)
                metrics[i] = value

        ref_metric = np.mean(metrics)
        print("Reference metric is ", ref_metric)

        return ref_metric
