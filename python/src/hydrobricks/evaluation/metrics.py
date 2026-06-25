from __future__ import annotations

import importlib

import numpy as np


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
