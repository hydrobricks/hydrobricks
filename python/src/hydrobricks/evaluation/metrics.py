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


# Skill metrics computed by hydrobricks itself rather than looked up in HydroErr.
# Mapped to their computing helper by :func:`evaluate`. The non-parametric KGE
# (Pool et al., 2018) is not provided by HydroErr, so it is routed to SPOTPY's
# implementation (the same one used as the calibration default). Both names map to
# the same score.
_KGE_NP_NAMES = frozenset({"kge_np", "kge_non_parametric"})


def is_error_metric(metric: str) -> bool:
    """Whether a HydroErr metric is an error (lower is better)."""
    return metric.lower() in _ERROR_METRICS


def _kge_non_parametric(simulation: np.ndarray, observation: np.ndarray) -> float:
    """Non-parametric Kling-Gupta Efficiency (Pool et al., 2018) via SPOTPY.

    HydroErr does not provide this variant, so it is delegated to
    ``spotpy.objectivefunctions.kge_non_parametric`` (the same function used as the
    :class:`~hydrobricks.trainer.SpotpySetup` default objective). SPOTPY is an optional
    dependency; a clear error is raised when it is missing.

    Note the argument order: SPOTPY expects ``kge_non_parametric(evaluation,
    simulation)``, i.e. ``(observation, simulation)`` — the reverse of the HydroErr
    convention used elsewhere in this module.
    """
    from hydrobricks._optional import HAS_SPOTPY, spotpy

    if not HAS_SPOTPY:
        raise ImportError(
            "The 'kge_np' (non-parametric KGE) metric requires the optional 'spotpy' "
            "package. Install it with 'pip install hydrobricks[all]' or "
            "'pip install spotpy'."
        )

    return spotpy.objectivefunctions.kge_non_parametric(observation, simulation)


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
        Examples: nse, kge_2012, ... In addition, ``'kge_np'`` (alias
        ``'kge_non_parametric'``) selects the non-parametric KGE (Pool et al., 2018),
        which is computed via the optional SPOTPY dependency.

    Returns
    -------
    The value of the selected metric.
    """
    if metric.lower() in _KGE_NP_NAMES:
        return _kge_non_parametric(simulation, observation)

    eval_fct = getattr(importlib.import_module("HydroErr"), metric)

    return eval_fct(simulation, observation)


def reference_value(observation: np.ndarray, metric: str) -> float:
    """Value of ``metric`` for the mean/climatology benchmark predictor.

    The benchmark predicts the mean of the observations everywhere; its metric value
    is the reference against which a model's error is turned into a skill score (for
    RMSE this is the standard deviation of the observations).

    Parameters
    ----------
    observation
        The observed values.
    metric
        The HydroErr metric name (see :func:`evaluate`).

    Returns
    -------
    The metric of the mean predictor.
    """
    observation = np.asarray(observation, dtype=float)
    benchmark = np.full(observation.shape, float(np.mean(observation)))
    return evaluate(benchmark, observation, metric)


def to_skill(value: float, metric: str, observation: np.ndarray) -> float:
    """Express a metric value as a benchmark-relative skill (higher is better).

    So that heterogeneous objectives (e.g. a discharge KGE and an auxiliary RMSE) can
    be combined on a comparable scale, every term is mapped to a skill where
    **1 = perfect** and **0 = no better than the mean/climatology benchmark**
    (negative = worse):

    - *Error* metrics (rmse, mae, ...) become ``1 - value / reference``, with
      ``reference`` the metric of the mean predictor (see :func:`reference_value`). For
      RMSE this is an NSE-like score.
    - *Skill* metrics (nse, kge*, r_squared, d, ...) are already in this form and
      returned unchanged.

    Parameters
    ----------
    value
        The metric value returned by :func:`evaluate` for the model.
    metric
        The HydroErr metric name.
    observation
        The observed values (used to compute the benchmark reference).

    Returns
    -------
    The benchmark-relative skill (or ``value`` unchanged for a skill metric).
    """
    if not is_error_metric(metric):
        return value
    reference = reference_value(observation, metric)
    if not np.isfinite(reference) or abs(reference) < 1e-12:
        # Degenerate benchmark (e.g. constant observations): fall back to the plain
        # negated error so the term stays finite and "higher is better".
        return -value
    return 1.0 - value / reference
