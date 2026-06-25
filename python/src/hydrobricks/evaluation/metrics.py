from __future__ import annotations

import importlib

import numpy as np

# HydroErr metrics for which a *lower* value is better (errors/distances). Anything
# not listed is assumed to be a skill score where a *higher* value is better
# (nse, kge*, r_squared, d, ...). Used to orient auxiliary objective terms.
_ERROR_METRICS = frozenset(
    {
        "me",
        "mae",
        "mse",
        "rmse",
        "rmsle",
        "mase",
        "mape",
        "mapd",
        "smape1",
        "smape2",
        "mad",
        "ed",
        "ned",
        "nrmse_range",
        "nrmse_mean",
        "nrmse_iqr",
        "irmse",
        "mle",
        "mdae",
        "g_mean_diff",
    }
)


def is_error_metric(metric: str) -> bool:
    """Whether a HydroErr metric is an error (lower is better)."""
    return metric.lower() in _ERROR_METRICS


def evaluate(simulation: np.ndarray, observation: np.ndarray, metric: str) -> float:
    """
    Evaluate the simulation using the provided metric (goodness of fit).

    Parameters
    ----------
    simulation
        The predicted time series.
    observation
        The time series of the observations with dates matching the simulated
        series.
    metric
        The abbreviation of the function as defined in HydroErr
        (https://hydroerr.readthedocs.io/en/stable/list_of_metrics.html)
        Examples: nse, kge_2012, ...

    Returns
    -------
    The value of the selected metric.
    """
    eval_fct = getattr(importlib.import_module("HydroErr"), metric)

    return eval_fct(simulation, observation)
