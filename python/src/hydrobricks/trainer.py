from __future__ import annotations

import importlib
import logging
import os
import uuid
from datetime import datetime
from typing import TYPE_CHECKING, Any, Callable

import numpy as np

from hydrobricks._exceptions import ConfigurationError, DataError, ModelError
from hydrobricks._optional import HAS_PATHOS, spotpy

logger = logging.getLogger(__name__)

# Process-scoped cache of the heavy, C++-backed objects (model, forcing, obs)
# built by a setup_factory. In parallel calibration SPOTPY pickles the work
# function (which carries the SpotpySetup) and ships it to workers *per task*, so
# every evaluation unpickles a fresh setup with `_built=False`. Without this
# cache each evaluation would rebuild the whole model from scratch. It is keyed by
# a per-calibration build token shared by all of a run's tasks, so the first task
# in a worker builds the objects and every later task in that process reuses them.
_BUILT_CACHE: dict = {}


def _format_params(params: Any) -> str:
    """Format a SPOTPY parameter set as compact ``name=value`` pairs.

    Accepts either a SPOTPY parameter object (with ``.name``/``.random``) or the
    ``(values, names)`` tuple SPOTPY passes to ``objectivefunction``.
    """
    if params is None:
        return "n/a"
    try:
        names = list(params.name)
        values = list(params.random)
    except AttributeError:
        try:
            values, names = list(params[0]), list(params[1])
        except Exception:
            return str(params)
    return ", ".join(f"{n}={float(v):.4g}" for n, v in zip(names, values))


def _enable_mpc_progress() -> None:
    """Make SPOTPY's ``mpc`` backend report progress while it runs.

    SPOTPY prints its progress line ("... of ..., min objf=..., time remaining:
    ...") from the main process inside the sampler's ``postprocessing`` loop, as
    it consumes results coming back from the worker pool. The ``mpc`` backend
    (``spotpy.parallel.mproc.ForEach``) collects every result with a *blocking*
    ``pool.map`` before yielding any of them, so that loop only runs once all
    repetitions have finished — i.e. the progress line never appears live, only
    in a burst at the very end.

    Switching to ``pool.imap`` (still ordered, but lazy) yields each result as
    soon as it is ready, so the main process prints progress incrementally, just
    like a sequential run. The patch is idempotent and leaves the unordered
    ``umpc`` backend (which already uses the lazy ``uimap``) untouched.
    """
    import spotpy.parallel.mproc as _mproc

    if getattr(_mproc.ForEach, "_hb_imap_progress", False):
        return

    def __call__(self, jobs):  # noqa: N807 (match SPOTPY's method name)
        yield from self.pool.imap(self.f, jobs)

    _mproc.ForEach.__call__ = __call__
    _mproc.ForEach._hb_imap_progress = True


if TYPE_CHECKING:
    from hydrobricks.forcing import Forcing
    from hydrobricks.models.model import Model
    from hydrobricks.observations import Observations
    from hydrobricks.parameters import ParameterSet


class SpotpySetup:
    """Setup class for SPOTPY optimization framework integration."""

    def __init__(
        self,
        model: Model | list[Model] | None = None,
        params: ParameterSet | None = None,
        forcing: Forcing | list[Forcing] | None = None,
        obs: Observations | list[Observations] | None = None,
        warmup: int = 365,
        obj_func: str | Callable[[np.ndarray, np.ndarray], float] | None = None,
        invert_obj_func: bool = False,
        dump_outputs: bool = False,
        dump_forcing: bool = False,
        dump_dir: str = "",
        setup_factory: Callable[[], tuple] | None = None,
    ) -> None:
        """
        Initialize SPOTPY setup for model calibration.

        For parallel calibration (SPOTPY ``parallel='mpc'``/``'mpi'``), the setup
        object is pickled and shipped to worker processes. The C++-backed model,
        forcing, and observation objects cannot be pickled, so use the
        ``setup_factory`` argument (or :meth:`from_factory`) to provide a picklable
        callable that rebuilds them inside each worker. See :meth:`from_factory`.

        Parameters
        ----------
        model
            Single Model instance or list of Model instances for ensemble calibration.
            Leave as None when using ``setup_factory``.
        params
            ParameterSet defining the model parameters to calibrate.
        forcing
            Single Forcing instance or list of Forcing instances (one per model).
            Leave as None when using ``setup_factory``.
        obs
            Single Observations instance or list for observed data (one per model).
            Leave as None when using ``setup_factory``.
        setup_factory
            Optional picklable callable taking no arguments and returning a
            ``(model, forcing, obs)`` tuple (each a single instance or a list).
            When provided, the heavy objects are built lazily (once per process),
            making the setup picklable for parallel calibration. Must be a
            top-level/module-level function so it can be pickled.
        warmup
            Number of days for model warmup period (skipped in evaluation).
            Default: 365
        obj_func
            Objective function for optimization. If None, uses non-parametric
            Kling-Gupta Efficiency. Can be a string (HydroErr function name) or
            callable that takes (observed, simulated) and returns a scalar.
            Default: None
        invert_obj_func
            If True, multiply objective function result by -1 (for minimizers).
            Default: False
        dump_outputs
            If True, save all simulation outputs to disk. Default: False
        dump_forcing
            If True, save forcing data used in simulations. Default: False
        dump_dir
            Directory path for saving dumped outputs. Default: '' (current directory)

        Raises
        ------
        ConfigurationError
            If invalid configuration parameters or mismatched ensemble sizes.
        DataError
            If observation data is invalid or missing.
        """
        if params is None:
            raise ConfigurationError(
                "Parameters cannot be None.",
                item_name="params",
                reason="Required parameter missing",
            )

        # Lightweight configuration is set up in every process (and after unpickling
        # in workers), so it must not depend on the heavy C++-backed objects.
        self.params = params
        self._setup_factory = setup_factory
        # Identifies this calibration's heavy objects in the process-scoped build
        # cache. It is pickled with the setup, so every worker task of this run
        # shares it and reuses the same model instead of rebuilding it each time.
        self._build_token = uuid.uuid4().hex

        # Validate and set configuration parameters
        self._validate_and_set_parameters(
            warmup, dump_dir, invert_obj_func, dump_outputs, dump_forcing
        )

        # Initialize SPOTPY parameters
        self.params_spotpy = params.get_for_spotpy()
        self.random_forcing = params.needs_random_forcing()

        # Validate objective function
        self._validate_and_set_objective_function(obj_func)

        # Heavy objects: either provided eagerly, or built lazily by the factory
        # (the latter keeps the setup picklable for parallel calibration).
        self.model = None
        self.forcing = None
        self.obs = None
        self._built = False
        if setup_factory is None:
            self._build_from_objects(model, forcing, obs)

    @classmethod
    def from_factory(
        cls,
        setup_factory: Callable[[], tuple],
        params: ParameterSet,
        **kwargs: Any,
    ) -> SpotpySetup:
        """
        Create a picklable setup for parallel calibration.

        The ``setup_factory`` is called once in each worker process to (re)build
        the model, forcing, and observations; the result is cached and reused
        across that worker's evaluations. This avoids pickling the C++-backed
        objects, which is required for SPOTPY ``parallel='mpc'``/``'mpi'`` (and
        in particular on Windows, where workers are spawned, not forked).

        Parameters
        ----------
        setup_factory
            Picklable callable taking no arguments and returning a
            ``(model, forcing, obs)`` tuple (each a single instance or a list).
            Must be a top-level/module-level function (lambdas and closures
            cannot be pickled by the standard backends).
        params
            ParameterSet defining the model parameters to calibrate. Must itself
            be picklable (plain hydrobricks ParameterSet instances are).
        **kwargs
            Forwarded to :class:`SpotpySetup` (``warmup``, ``obj_func``,
            ``invert_obj_func``, ``dump_outputs``, ``dump_forcing``, ``dump_dir``).

        Returns
        -------
        SpotpySetup
            A setup that builds its models lazily and can be pickled to workers.
        """
        return cls(params=params, setup_factory=setup_factory, **kwargs)

    def _build_from_objects(
        self,
        model: Model | list[Model] | None,
        forcing: Forcing | list[Forcing] | None,
        obs: Observations | list[Observations] | None,
    ) -> None:
        """Normalize, validate, and wire up the concrete model/forcing/obs objects."""
        self.model = self._normalize_model_list(model)
        self.forcing = self._normalize_forcing_list(forcing)
        self.obs = self._normalize_observations_list(obs)

        # Validate ensemble sizes match
        self._validate_ensemble_sizes()

        # Setup forcing and models
        self._setup_forcing_and_models()

        self._built = True

    def _ensure_built(self) -> None:
        """Build the heavy objects on first use, reusing them across the worker.

        In parallel calibration SPOTPY pickles the work function (which carries
        this setup) and ships it to workers *per task*, so each evaluation
        unpickles a fresh setup with ``_built=False``. Rebuilding the C++
        model/forcing/obs every time would dominate the run time, so the built
        objects are cached at process scope (see :data:`_BUILT_CACHE`) and keyed
        by a per-calibration token shared by all tasks. The first task in a worker
        builds them; every later task in that process reuses them.
        """
        if self._built:
            return
        if self._setup_factory is None:
            raise ModelError(
                "SpotpySetup has no models and no setup_factory to build them.",
                is_initialized=False,
            )

        # Reuse objects already built by an earlier task in this worker process.
        cached = _BUILT_CACHE.get(self._build_token)
        if cached is not None:
            self.model, self.forcing, self.obs = cached
            self._built = True
            return

        result = self._setup_factory()
        # Accept either (model, forcing, obs) or (model, parameters, forcing, obs);
        # the parameters are ignored here (this instance already holds them).
        if not isinstance(result, tuple) or len(result) not in (3, 4):
            raise ConfigurationError(
                "setup_factory must return a (model, forcing, obs) or "
                "(model, parameters, forcing, obs) tuple.",
                item_name="setup_factory",
                reason="Invalid factory return value",
            )
        if len(result) == 4:
            model, _, forcing, obs = result
        else:
            model, forcing, obs = result
        self._build_from_objects(model, forcing, obs)

        # Cache for the other evaluations this worker will handle. Only the
        # current calibration is kept (a worker processes one run at a time), so
        # the cache cannot grow or hand back another run's stale objects.
        _BUILT_CACHE.clear()
        _BUILT_CACHE[self._build_token] = (self.model, self.forcing, self.obs)

    def __getstate__(self) -> dict:
        """Return picklable state, dropping the C++-backed objects for workers."""
        if self._setup_factory is None:
            raise TypeError(
                "This SpotpySetup was built from concrete model objects, which "
                "cannot be pickled for parallel calibration. Build it with "
                "SpotpySetup.from_factory(...) (or pass setup_factory=...) so the "
                "models can be rebuilt in each worker process."
            )
        state = self.__dict__.copy()
        # Each worker rebuilds these via the factory on first use.
        state["model"] = None
        state["forcing"] = None
        state["obs"] = None
        state["_built"] = False
        return state

    def __setstate__(self, state: dict) -> None:
        self.__dict__.update(state)

    def _normalize_model_list(self, model: Model | list[Model]) -> list[Model]:
        """
        Validate and normalize model input to a list.

        Parameters
        ----------
        model
            Single Model instance or list of Model instances.

        Returns
        -------
        list[Model]
            Validated list of Model instances.

        Raises
        ------
        ConfigurationError
            If model is None or empty list.
        """
        if model is None:
            raise ConfigurationError(
                "Model cannot be None.",
                item_name="model",
                reason="Required parameter missing",
            )
        models = [model] if not isinstance(model, list) else model
        if len(models) == 0:
            raise ConfigurationError(
                "Model list cannot be empty.",
                item_name="model",
                reason="Empty model list",
            )
        return models

    def _normalize_forcing_list(
        self, forcing: Forcing | list[Forcing]
    ) -> list[Forcing]:
        """
        Validate and normalize forcing input to a list.

        Parameters
        ----------
        forcing
            Single Forcing instance or list of Forcing instances.

        Returns
        -------
        list[Forcing]
            Validated list of Forcing instances.

        Raises
        ------
        ConfigurationError
            If forcing is None or empty list.
        """
        if forcing is None:
            raise ConfigurationError(
                "Forcing cannot be None.",
                item_name="forcing",
                reason="Required parameter missing",
            )
        forcings = [forcing] if not isinstance(forcing, list) else forcing
        if len(forcings) == 0:
            raise ConfigurationError(
                "Forcing list cannot be empty.",
                item_name="forcing",
                reason="Empty forcing list",
            )
        return forcings

    def _normalize_observations_list(
        self, obs: Observations | list[Observations]
    ) -> list[np.ndarray]:
        """
        Validate and normalize observations to a list of numpy arrays.

        Parameters
        ----------
        obs
            Single Observations instance or list of Observations instances.

        Returns
        -------
        list[np.ndarray]
            Validated list of observation data arrays.

        Raises
        ------
        ConfigurationError
            If obs is None or empty list.
        DataError
            If observation data is missing, invalid, or empty.
        """
        if obs is None:
            raise ConfigurationError(
                "Observations cannot be None.",
                item_name="obs",
                reason="Required parameter missing",
            )
        obs_list = [obs] if not isinstance(obs, list) else obs
        if len(obs_list) == 0:
            raise ConfigurationError(
                "Observations list cannot be empty.",
                item_name="obs",
                reason="Empty observations list",
            )

        observations = []
        for idx, o in enumerate(obs_list):
            if not hasattr(o, "data") or o.data is None:
                raise DataError(
                    f"Observations at index {idx} has no data attribute.",
                    data_type="observations",
                    reason="Missing or invalid observation data",
                )
            if len(o.data) == 0:
                raise DataError(
                    f"Observations at index {idx} has empty data.",
                    data_type="observations",
                    reason="Empty observation dataset",
                )
            observations.append(o.data[0])
        return observations

    def _validate_ensemble_sizes(self) -> None:
        """
        Validate that models, forcing, and observations have matching sizes.

        Checks that the number of models equals the number of forcing instances
        and the number of observation datasets. Raises specific errors if sizes
        don't match.

        Raises
        ------
        ConfigurationError
            If len(models) != len(forcing) or len(models) != len(observations),
            with details about which counts don't match.
        """
        num_models = len(self.model)
        num_forcing = len(self.forcing)
        num_obs = len(self.obs)

        if num_models != num_forcing:
            raise ConfigurationError(
                f"Number of models ({num_models}) does not match "
                f"number of forcing datasets ({num_forcing}).",
                reason="Mismatched ensemble sizes",
            )
        if num_models != num_obs:
            raise ConfigurationError(
                f"Number of models ({num_models}) does not match "
                f"number of observation datasets ({num_obs}).",
                reason="Mismatched ensemble sizes",
            )

    def _validate_and_set_parameters(
        self,
        warmup: int,
        dump_dir: str,
        invert_obj_func: bool,
        dump_outputs: bool,
        dump_forcing: bool,
    ) -> None:
        """
        Validate and set configuration parameters.

        Validates the warmup period and dump directory parameters, then sets
        all configuration attributes on the instance.

        Parameters
        ----------
        warmup
            Number of days for model warmup period. Must be non-negative integer.
        dump_dir
            Directory path for saving dumped outputs. Only validated/created
            if dump_outputs or dump_forcing is True.
        invert_obj_func
            If True, multiply objective function result by -1 (for minimizers).
        dump_outputs
            If True, save all simulation outputs to disk.
        dump_forcing
            If True, save forcing data used in simulations.

        Raises
        ------
        ConfigurationError
            If warmup is not a non-negative integer or if dump_dir cannot be
            created/accessed.
        """
        if not isinstance(warmup, int) or warmup < 0:
            raise ConfigurationError(
                f"Warmup period must be non-negative integer, got {warmup}.",
                item_name="warmup",
                item_value=warmup,
                reason="Invalid warmup configuration",
            )
        self.warmup = warmup

        if (dump_outputs or dump_forcing) and dump_dir:
            try:
                dump_path = os.path.abspath(dump_dir)
                os.makedirs(dump_path, exist_ok=True)
                self.dump_dir = dump_path
            except OSError as e:
                raise ConfigurationError(
                    f'Cannot create dump directory "{dump_dir}": {e}',
                    item_name="dump_dir",
                    item_value=dump_dir,
                    reason="Directory access error",
                ) from e
        else:
            self.dump_dir = ""

        self.dump_outputs = dump_outputs
        self.dump_forcing = dump_forcing
        self.invert_obj_func = invert_obj_func

    def _setup_forcing_and_models(self) -> None:
        """
        Apply forcing operations and configure models.

        Applies parameter-dependent operations to all forcing datasets and
        configures models with forcing data if not using random forcing.

        Side Effects
        -----------
        - Modifies all forcing datasets via apply_operations()
        - Configures models with forcing via set_forcing() (if not random_forcing)

        Raises
        ------
        ModelError
            If forcing operations or model configuration fails. Original exception
            is preserved via exception chaining.
        """
        try:
            for f in self.forcing:
                f.apply_operations(self.params)
            if not self.random_forcing:
                for m, f in zip(self.model, self.forcing):
                    m.set_forcing(forcing=f)
        except Exception as e:
            raise ModelError(
                f"Failed to setup forcing and models: {e}", is_initialized=False
            ) from e

    def _validate_and_set_objective_function(
        self, obj_func: str | Callable[[np.ndarray, np.ndarray], float] | None
    ) -> None:
        """
        Validate and set the objective function.

        Parameters
        ----------
        obj_func
            Objective function specification. Can be:
            - None: Use default non-parametric Kling-Gupta Efficiency
            - str: Name of HydroErr metric (e.g., 'nse', 'kge_2012')
            - callable: User-defined function with signature
              (observed, simulated) -> float

        Raises
        ------
        ConfigurationError
            If obj_func is not None, string, or callable.
        """
        if obj_func is None:
            logger.info("Objective function: Non parametric Kling-Gupta Efficiency")
            self.obj_func = None
        elif isinstance(obj_func, str):
            self.obj_func = obj_func
        elif callable(obj_func):
            self.obj_func = obj_func
        else:
            raise ConfigurationError(
                f"Objective function must be None, string, or callable, "
                f"got {type(obj_func).__name__}.",
                item_name="obj_func",
                item_value=obj_func,
                reason="Invalid objective function type",
            )

    def parameters(self) -> Any:
        """
        Generate random parameter sets that satisfy constraints.

        Returns
        -------
        Any
            SPOTPY parameter object with valid random parameter values.

        Raises
        ------
        RuntimeError
            If unable to generate valid parameters after 1000 attempts.
        """
        x = None
        for i in range(1000):
            x = spotpy.parameter.generate(self.params_spotpy)
            names = [row[1] for row in x]
            values = [row[0] for row in x]
            params = self.params
            param_values = dict(zip(names, values))
            # Spotpy samples are in transformed (optimizer) space; back-transform.
            params.set_values(param_values, check_range=False, transformed=True)

            if params.constraints_satisfied() and params.range_satisfied():
                break

            if i >= 1000:
                raise ModelError("The parameter constraints could not be satisfied.")

        return x

    def simulation(self, x: spotpy.parameter) -> list[np.ndarray] | None:
        """
        Run model simulation with given parameter set.

        Parameters
        ----------
        x
            SPOTPY parameter object containing parameter values.

        Returns
        -------
        list[np.ndarray] | None
            List of simulated discharge time series (one per model), or None
            if simulation failed or constraints were violated.
        """
        self._ensure_built()
        params = self.params
        param_values = dict(zip(x.name, x.random))
        logger.debug(f"Setting {len(param_values)} parameter values for simulation")
        # Spotpy samples are in transformed (optimizer) space; back-transform.
        # Skip range checking here so an out-of-range draw (e.g. from an unbounded
        # prior) is rejected cleanly below rather than raising and crashing the
        # sampler.
        params.set_values(param_values, check_range=False, transformed=True)

        if not params.constraints_satisfied():
            logger.warning(
                "Skipped run: parameter constraints not satisfied (%s)",
                _format_params(x),
            )
            return None

        if not params.range_satisfied():
            logger.warning(
                "Skipped run: parameter(s) out of range (%s)",
                _format_params(x),
            )
            return None

        logger.debug(f"Running {len(self.model)} model(s)")
        all_sim = []
        for model_idx, (model, forcing) in enumerate(zip(self.model, self.forcing)):
            logger.debug(f"  Model {model_idx}: running simulation")

            try:
                if self.random_forcing:
                    logger.debug("    Applying random forcing operations")
                    forcing.apply_operations(params, apply_to_all=False)
                    model.run(parameters=params, forcing=forcing)
                else:
                    model.run(parameters=params)
                sim = model.get_outlet_discharge()
            except Exception as e:
                # One bad parameter set must not abort the whole calibration.
                logger.warning(
                    "Skipped run: model %d failed (%s): %s",
                    model_idx,
                    _format_params(x),
                    e,
                )
                return None

            if sim.size == 0:
                logger.warning(
                    "Skipped run: model %d produced no discharge (%s)",
                    model_idx,
                    _format_params(x),
                )
                return None

            logger.debug(f"  Model {model_idx}: extracted {len(sim)} discharge values")
            all_sim.append(sim[self.warmup :])

            if self.dump_outputs or self.dump_forcing:
                now = datetime.now()
                # Include the PID and a random suffix so concurrent workers (and
                # rapid successive runs) never collide on the same output folder.
                date_time = now.strftime("%Y-%m-%d_%H%M%S_%f")
                unique = f"{date_time}_{os.getpid()}_{uuid.uuid4().hex[:8]}"
                path = os.path.join(self.dump_dir, unique)
                os.makedirs(path, exist_ok=True)
                if self.dump_outputs:
                    logger.debug(f"    Dumping outputs to {path}")
                    model.dump_outputs(path)
                if self.dump_forcing:
                    logger.debug(f"    Saving forcing to {path}")
                    forcing.save_as(os.path.join(path, f"forcing_{model_idx}.nc"))

        logger.debug(f"Simulation complete: {len(all_sim)} models processed")
        return all_sim

    def evaluation(self) -> list[np.ndarray]:
        self._ensure_built()
        all_obs = []
        for obs in self.obs:
            all_obs.append(obs[self.warmup :])

        return all_obs

    def objectivefunction(
        self,
        simulation: list[np.ndarray],
        evaluation: list[np.ndarray],
        params: spotpy.parameter | None = None,
    ) -> float:
        if simulation is None:
            # The reason (constraints / range / no discharge) is already logged by
            # simulation(); here the set is just scored as worst so the optimizer
            # discards it. Keep this at debug to avoid duplicate noise.
            logger.debug(
                "Scoring rejected parameter set as worst (%s).", _format_params(params)
            )
            if self.invert_obj_func:
                return np.inf
            return -np.inf

        all_like = []
        for sim, obs in zip(simulation, evaluation):
            if sim is None or sim.size == 0:
                logger.debug(
                    "Scoring empty simulation as worst (%s).", _format_params(params)
                )
                like = -np.inf
            elif len(sim) != len(obs):
                logger.warning(
                    "Simulation and observation lengths differ "
                    "(sim=%d, obs=%d, params: %s).",
                    len(sim),
                    len(obs),
                    _format_params(params),
                )
                like = -np.inf
            else:
                if not self.obj_func:
                    like = spotpy.objectivefunctions.kge_non_parametric(obs, sim)
                elif isinstance(self.obj_func, str):
                    like = evaluate(sim, obs, self.obj_func)
                else:
                    like = self.obj_func(obs, sim)

            if self.invert_obj_func:
                like = -like

            all_like.append(like)

        # Return the mean of the objective function
        return np.mean(all_like)


def calibrate(
    spot_setup: SpotpySetup,
    algorithm: str,
    repetitions: int,
    dbname: str | None = None,
    dbformat: str = "ram",
    parallel: str = "seq",
    save_sim: bool = True,
    n_workers: int | None = None,
    **algorithm_kwargs: Any,
) -> Any:
    """
    Run a SPOTPY calibration, optionally across multiple processes.

    Thin convenience wrapper around the SPOTPY algorithm classes that validates
    the requested parallel backend before sampling. For ``parallel='mpc'`` the
    ``spot_setup`` must have been built with :meth:`SpotpySetup.from_factory` so
    that models can be rebuilt in each worker process.

    Parameters
    ----------
    spot_setup
        The configured :class:`SpotpySetup`.
    algorithm
        Name of the SPOTPY algorithm (e.g. ``'mc'``, ``'lhs'``, ``'sceua'``,
        ``'dream'``), resolved from ``spotpy.algorithms``.
    repetitions
        Number of repetitions passed to ``sampler.sample()``.
    dbname
        SPOTPY database name (passed through to the algorithm).
    dbformat
        SPOTPY database format (``'ram'``, ``'csv'``, ...). Default: ``'ram'``.
    parallel
        SPOTPY parallel backend: ``'seq'`` (default), ``'mpc'`` (multiprocessing,
        requires the ``pathos`` package), ``'mpi'`` (requires ``mpi4py``), or
        ``'umpc'``. Note that not all algorithms parallelize well: independent
        samplers (``mc``, ``lhs``, ``rope``) and ``dream`` benefit most, whereas
        SCE-UA is largely sequential.
    save_sim
        Whether SPOTPY stores the simulated series. Default: True.
    n_workers
        Number of worker processes for ``parallel='mpc'``/``'umpc'``. Defaults to
        None, i.e. all logical CPUs. Ignored for ``'seq'`` and ``'mpi'`` (the MPI
        process count is set by the MPI launcher, e.g. ``mpiexec -n``).
    **algorithm_kwargs
        Extra keyword arguments forwarded to the SPOTPY algorithm constructor.

    Returns
    -------
    The SPOTPY sampler instance (call ``sampler.getdata()`` for results).
    """
    if parallel in ("mpc", "umpc") and not HAS_PATHOS:
        raise ConfigurationError(
            "parallel='mpc' requires the 'pathos' package. "
            "Install it with: pip install pathos",
            item_name="parallel",
            item_value=parallel,
            reason="Missing optional dependency",
        )
    if parallel != "seq" and spot_setup._setup_factory is None:
        raise ConfigurationError(
            "Parallel calibration requires a SpotpySetup built via "
            "SpotpySetup.from_factory(...) so models can be rebuilt in worker "
            "processes.",
            item_name="spot_setup",
            reason="Setup is not picklable for parallel runs",
        )

    if parallel == "mpc":
        # SPOTPY's mpc backend blocks on pool.map and only reports progress once
        # everything is done; stream results so progress prints live (see helper).
        _enable_mpc_progress()

    if n_workers is not None:
        if not isinstance(n_workers, int) or n_workers < 1:
            raise ConfigurationError(
                f"n_workers must be a positive integer, got {n_workers}.",
                item_name="n_workers",
                item_value=n_workers,
                reason="Invalid worker count",
            )
        if parallel in ("mpc", "umpc"):
            # SPOTPY's multiprocessing backends read the pool size from a module-level
            # global, set before the pool is created in sampler.sample(). The two
            # backends use separate globals, so set whichever applies.
            import spotpy.parallel.mproc as _mproc
            import spotpy.parallel.umproc as _umproc

            _mproc.process_count = n_workers
            _umproc.process_count = n_workers
        else:
            logger.warning(
                "n_workers is ignored for parallel='%s' (only applies to "
                "'mpc'/'umpc').",
                parallel,
            )

    algorithm_cls = getattr(spotpy.algorithms, algorithm)
    sampler = algorithm_cls(
        spot_setup,
        dbname=dbname,
        dbformat=dbformat,
        parallel=parallel,
        save_sim=save_sim,
        **algorithm_kwargs,
    )
    sampler.sample(repetitions)

    return sampler


def calibrate_from_factory(
    setup_factory: Callable[[], tuple],
    algorithm: str,
    repetitions: int,
    allow_changing: list[str] | None = None,
    warmup: int = 365,
    obj_func: str | Callable[[np.ndarray, np.ndarray], float] | None = None,
    invert_obj_func: bool = False,
    dump_outputs: bool = False,
    dump_forcing: bool = False,
    dump_dir: str = "",
    dbname: str | None = None,
    dbformat: str = "ram",
    parallel: str = "seq",
    save_sim: bool = True,
    n_workers: int | None = None,
    **algorithm_kwargs: Any,
) -> Any:
    """
    Build a calibration setup from a single factory and run it (parallel-ready).

    This is the simplest way to run a calibration, including in parallel: provide
    one factory that builds everything, and this function assembles the
    (picklable) :class:`SpotpySetup` and runs the sampler. It removes the
    boilerplate of extracting the parameters, calling
    :meth:`SpotpySetup.from_factory`, and then :func:`calibrate` separately.

    For ``parallel='mpc'``/``'mpi'`` the factory is shipped to each worker process
    to rebuild the model there, so it must be a top-level (module-level) function
    — not a lambda or closure — and the call must be guarded by
    ``if __name__ == '__main__':`` (required on platforms that spawn workers, such
    as Windows).

    Parameters
    ----------
    setup_factory
        Callable taking no arguments and returning a
        ``(model, parameters, forcing, obs)`` tuple. Called once in the main
        process (to obtain the parameters and build the local setup) and once in
        each worker (to rebuild the model). Must be picklable for parallel runs.
    algorithm
        Name of the SPOTPY algorithm (e.g. ``'mc'``, ``'lhs'``, ``'sceua'``).
    repetitions
        Number of repetitions passed to ``sampler.sample()``.
    allow_changing
        Optional list of parameter names/aliases to calibrate. If given, it
        overrides any ``parameters.allow_changing`` set inside the factory.
    warmup, obj_func, invert_obj_func, dump_outputs, dump_forcing, dump_dir
        Forwarded to :class:`SpotpySetup`.
    dbname, dbformat, parallel, save_sim, n_workers, **algorithm_kwargs
        Forwarded to :func:`calibrate`.

    Returns
    -------
    The SPOTPY sampler instance (call ``sampler.getdata()`` for results).
    """
    built = setup_factory()
    if not isinstance(built, tuple) or len(built) != 4:
        raise ConfigurationError(
            "setup_factory must return a (model, parameters, forcing, obs) tuple.",
            item_name="setup_factory",
            reason="Invalid factory return value",
        )
    model, params, forcing, obs = built
    if allow_changing is not None:
        params.allow_changing = allow_changing

    spot_setup = SpotpySetup.from_factory(
        setup_factory,
        params,
        warmup=warmup,
        obj_func=obj_func,
        invert_obj_func=invert_obj_func,
        dump_outputs=dump_outputs,
        dump_forcing=dump_forcing,
        dump_dir=dump_dir,
    )
    # Reuse the objects already built in this (main) process to avoid building
    # them twice here; workers rebuild via the factory after unpickling (the
    # heavy objects are dropped by SpotpySetup.__getstate__).
    spot_setup._build_from_objects(model, forcing, obs)

    return calibrate(
        spot_setup,
        algorithm,
        repetitions,
        dbname=dbname,
        dbformat=dbformat,
        parallel=parallel,
        save_sim=save_sim,
        n_workers=n_workers,
        **algorithm_kwargs,
    )


def evaluate(simulation: np.array, observation: np.array, metric: str) -> float:
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
