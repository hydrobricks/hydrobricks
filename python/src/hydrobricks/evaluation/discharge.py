from __future__ import annotations

import itertools
import logging
from datetime import datetime
from pathlib import Path

import numpy as np
import pandas as pd

from hydrobricks._exceptions import DataError
from hydrobricks.evaluation.metrics import evaluate
from hydrobricks.periods import Period, Periods
from hydrobricks.time_series import TimeSeries1D

logger = logging.getLogger(__name__)


class DischargeObservations(TimeSeries1D):
    """Observed discharge time series (the primary calibration signal)."""

    def __init__(
        self,
        start_date: str | pd.Timestamp | datetime | Period | Periods,
        end_date: str | pd.Timestamp | datetime | None = None,
    ) -> None:
        """Initialize DischargeObservations instance.

        Parameters
        ----------
        start_date, end_date
            Bounds of the period of interest. :meth:`load_from_csv` restricts the
            loaded data to this range by default, so the calibration observations
            match the simulation period without manual pre-slicing. These are also
            the natural reference to pass as ``start_date``/``end_date`` to any
            auxiliary observation (e.g. ``GlacierMassBalanceObservations.from_glamos``,
            ``SnowCoverObservations.from_modis``), so every signal is restricted to
            the same period. Alternatively, pass a single
            :class:`~hydrobricks.periods.Period` or
            :class:`~hydrobricks.periods.Periods` (its full span is used), so the
            observations are loaded once for all periods.
        """
        super().__init__()
        if isinstance(start_date, Periods):
            start_date = start_date.full_span
        if isinstance(start_date, Period):
            start_date, end_date = start_date.start, start_date.end
        if end_date is None:
            raise DataError(
                "An end date is required (or pass a Period/Periods).",
                data_type="observations",
                reason="Missing end date",
            )
        self.start_date = pd.Timestamp(start_date)
        self.end_date = pd.Timestamp(end_date)

    def load_from_csv(
        self,
        path: str | Path,
        column_time: str,
        time_format: str,
        content: dict[str, str],
        start_date: str | pd.Timestamp | datetime | None = None,
        end_date: str | pd.Timestamp | datetime | None = None,
    ) -> None:
        """
        Read discharge observations from a CSV file.

        Restricted by default to ``[start_date, end_date]`` as given to the
        constructor, so the loaded series already matches the simulation period
        (no manual pre-slicing needed). Pass ``start_date``/``end_date`` here to
        load a different range instead (e.g. to deliberately load a period wider
        than the constructor's, for testing).

        Parameters
        ----------
        path
            Path to the CSV file containing the discharge data.
        column_time
            Column name containing the time values.
        time_format
            Format string for parsing time values (e.g., '%Y-%m-%d').
        content
            Dictionary mapping variable names/enums to column names in the CSV.
            Example: {'discharge': 'Discharge (mm/d)'}
        start_date, end_date
            Overrides the constructor's ``start_date``/``end_date`` for this load.

        Raises
        ------
        FileNotFoundError
            If the specified file does not exist.
        KeyError
            If required columns are not found in the CSV file.

        Examples
        --------
        >>> obs = DischargeObservations('2000-01-01', '2010-12-31')
        >>> obs.load_from_csv('data.csv', 'date', '%Y-%m-%d', {'discharge': 'Q'})
        """
        super().load_from_csv(
            path,
            column_time,
            time_format,
            content,
            start_date=start_date if start_date is not None else self.start_date,
            end_date=end_date if end_date is not None else self.end_date,
        )

    def compute_reference_metric(
        self,
        metric: str,
        start_date: str | None = None,
        end_date: str | None = None,
        with_exclusion: bool = False,
        mean_discharge: bool = False,
        all_combinations: bool = False,
        n_evals: int = 100,
    ) -> float:
        """
        Compute a reference for the provided metric (goodness of fit)
        by block bootstrapping the observed series n_evals times (100 times by default),
        evaluating the bootstrapped series using the provided metric and computing
        the mean of the results.

        Parameters
        ----------
        metric
            The abbreviation of the function as defined in HydroErr
            (https://hydroerr.readthedocs.io/en/stable/list_of_metrics.html)
            Examples: 'nse', 'kge_2012', 'rmse', etc. Also accepts 'kge_np' for the
            non-parametric KGE (Pool et al., 2018; requires the optional SPOTPY).
        start_date
            Start date string for period of interest (format: 'YYYY-MM-DD').
            If None, uses full time series. Default: None
        end_date
            End date string for period of interest (format: 'YYYY-MM-DD').
            If None, uses full time series. Default: None
        with_exclusion
            If True, avoid using the same year's data for the same position in the
            bootstrapped sample, ensuring no self-selection for specific years.
            Default: False
        mean_discharge
            If True, computes the average on the discharge directly rather than on
            the result of the HydroErr function. Default: False
        all_combinations
            If True uses all combinations possible for the bootstrapping.
            If False, randomly samples n_evals combinations. Default: False
        n_evals
            Number of random evaluations to perform (ignored if all_combinations=True).
            Default: 100

        Returns
        -------
        float
            The mean value of n_evals realizations of the selected metric.

        Raises
        ------
        DataError
            If there is only one year of data (insufficient for block bootstrapping).
        ValueError
            If metric is not recognized or if time series setup is invalid.

        Examples
        --------
        >>> obs = DischargeObservations('2000-01-01', '2010-12-31')
        >>> obs.load_from_csv('data.csv', 'date', '%Y-%m-%d', {'discharge': 'Q'})
        >>> ref_metric = obs.compute_reference_metric('nse', n_evals=100)
        """
        # Prepare base dataframe
        df, years = self._prepare_dataframe()

        # Handle mean discharge computation separately
        if mean_discharge:
            return self._compute_mean_discharge_metric(df, metric, start_date, end_date)

        # Validate sufficient data
        self._validate_years_for_bootstrapping(years)

        # Prepare comparison data
        df, comparing_years, comparing_df = self._prepare_comparison_data(
            df, years, start_date, end_date
        )

        # Compute metrics via bootstrapping
        if all_combinations:
            metrics = self._compute_all_combinations(
                df, years, comparing_years, comparing_df, metric, with_exclusion
            )
        else:
            metrics = self._compute_random_sampling(
                df,
                years,
                comparing_years,
                comparing_df,
                metric,
                with_exclusion,
                n_evals,
            )

        ref_metric = float(np.mean(metrics))
        logger.info(f"Reference metric computed: {ref_metric}")

        return ref_metric

    def _prepare_dataframe(self) -> tuple[pd.DataFrame, np.ndarray]:
        """
        Prepare the base dataframe with time series data and years.

        Returns
        -------
        tuple[pd.DataFrame, np.ndarray]
            DataFrame with data indexed by date, and array of unique years.
        """
        df = self.time.to_frame(name="date").copy()
        df["data"] = self.data[0]
        df["year"] = pd.DatetimeIndex(df["date"]).year
        df = df.set_index("date")

        # Remove February 29 to ensure all years have 365 days
        df = df[df.index.strftime("%m-%d") != "02-29"]
        years = df.year.unique()

        return df, years

    def _compute_mean_discharge_metric(
        self,
        df: pd.DataFrame,
        metric: str,
        start_date: str | None,
        end_date: str | None,
    ) -> float:
        """
        Compute metric using mean discharge grouped by day of year.

        Parameters
        ----------
        df
            DataFrame with observation data
        metric
            Metric name to evaluate
        start_date
            Optional start date for filtering
        end_date
            Optional end date for filtering

        Returns
        -------
        float
            Computed metric value
        """
        # Use groupby to compute mean discharge per day of year
        df["day_of_year"] = df.index.strftime("%m-%d")
        df["mean_discharge"] = df.groupby("day_of_year")["data"].transform("mean")

        # Apply date filtering if specified
        if start_date:
            df = df[df.index >= start_date]
        if end_date:
            df = df[df.index <= end_date]

        return evaluate(df.mean_discharge.values, df.data.values, metric)

    def _validate_years_for_bootstrapping(self, years: np.ndarray) -> None:
        """
        Validate that sufficient years are available for bootstrapping.

        Parameters
        ----------
        years
            Array of unique years in the dataset

        Raises
        ------
        DataError
            If fewer than 2 years of data are available
        """
        if len(years) < 2:
            raise DataError(
                "At least two years of data are required for block bootstrapping. "
                f"Found only {len(years)} year(s).",
                data_type="observations",
                reason="Insufficient years for metric computation",
            )

    def _prepare_comparison_data(
        self,
        df: pd.DataFrame,
        years: np.ndarray,
        start_date: str | None,
        end_date: str | None,
    ) -> tuple[pd.DataFrame, np.ndarray, pd.DataFrame]:
        """
        Prepare data for comparison based on date range.

        Parameters
        ----------
        df
            DataFrame indexed by date
        years
            Array of all available years
        start_date
            Optional start date
        end_date
            Optional end date

        Returns
        -------
        tuple[pd.DataFrame, np.ndarray, pd.DataFrame]
            Tuple of (df indexed by year, comparing years, comparison dataframe)
        """
        df = df.reset_index().set_index("year")

        if start_date and end_date:
            date_range = pd.date_range(start_date, end_date)
            comparing_years = np.unique(date_range.year)

            comparing_df = df.set_index("date")
            comparing_df = comparing_df[
                (comparing_df.index >= start_date) & (comparing_df.index <= end_date)
            ]
        else:
            comparing_years = years
            comparing_df = df

        return df, comparing_years, comparing_df

    def _should_exclude_sample(
        self, sampled_years: np.ndarray, comparing_years: np.ndarray
    ) -> bool:
        """
        Check if a sample should be excluded based on exclusion criteria.

        Parameters
        ----------
        sampled_years
            Years selected in the current bootstrap sample
        comparing_years
            Years in the comparison dataset

        Returns
        -------
        bool
            True if sample should be excluded, False otherwise
        """
        diff = sampled_years - comparing_years
        return not np.all(diff)

    def _compute_all_combinations(
        self,
        df: pd.DataFrame,
        years: np.ndarray,
        comparing_years: np.ndarray,
        comparing_df: pd.DataFrame,
        metric: str,
        with_exclusion: bool,
    ) -> list[float]:
        """
        Compute metrics for all possible year combinations.

        Parameters
        ----------
        df
            DataFrame indexed by year
        years
            All available years
        comparing_years
            Years to compare against
        comparing_df
            DataFrame for comparison
        metric
            Metric to evaluate
        with_exclusion
            Whether to exclude self-selection

        Returns
        -------
        list[float]
            List of computed metric values
        """
        metrics = []
        year_combinations = itertools.product(years, repeat=len(comparing_years))

        for sampled_years_tuple in year_combinations:
            sampled_years = np.array(sampled_years_tuple)

            if with_exclusion and self._should_exclude_sample(
                sampled_years, comparing_years
            ):
                continue

            new_df = df.loc[sampled_years].copy()
            value = evaluate(new_df.data.values, comparing_df.data.values, metric)
            metrics.append(value)

        return metrics

    def _compute_random_sampling(
        self,
        df: pd.DataFrame,
        years: np.ndarray,
        comparing_years: np.ndarray,
        comparing_df: pd.DataFrame,
        metric: str,
        with_exclusion: bool,
        n_evals: int,
    ) -> np.ndarray:
        """
        Compute metrics using random sampling of year combinations.

        Parameters
        ----------
        df
            DataFrame indexed by year
        years
            All available years
        comparing_years
            Years to compare against
        comparing_df
            DataFrame for comparison
        metric
            Metric to evaluate
        with_exclusion
            Whether to exclude self-selection
        n_evals
            Number of random evaluations

        Returns
        -------
        np.ndarray
            Array of computed metric values
        """
        metrics = []

        while len(metrics) < n_evals:
            sampled_years = np.random.choice(
                years, size=len(comparing_years), replace=True
            )

            if with_exclusion and self._should_exclude_sample(
                sampled_years, comparing_years
            ):
                continue

            new_df = df.loc[sampled_years].copy()
            value = evaluate(new_df.data.values, comparing_df.data.values, metric)
            metrics.append(value)

        return np.array(metrics)
