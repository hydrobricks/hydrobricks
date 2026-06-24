import logging
import sys

import matplotlib.pyplot as plt
import spotpy

import hydrobricks.trainer as trainer
from examples._helpers.models_setup_helper import ModelSetupHelper

logging.basicConfig(
    level=logging.INFO,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

# Parameters varied during the Monte Carlo analysis.
ALLOW_CHANGING = [
    "a_snow",
    "k_quick",
    "A",
    "k_slow_1",
    "percol",
    "k_slow_2",
    "precip_corr_factor",
]


def build_all():
    """Build the model, parameters, forcing, and observations from the test data."""
    helper = ModelSetupHelper(
        "ch_sitter_appenzell", start_date="1981-01-01", end_date="2020-12-31"
    )
    helper.create_hydro_units_from_csv_file(filename="hydro_units_elevation.csv")
    forcing = helper.get_forcing_data_from_csv_file(
        ref_elevation=1253, use_precip_gradient=True
    )
    obs = helper.get_obs_data_from_csv_file()
    socont, parameters = helper.get_model_and_params_socont()

    # Select the parameters to optimize/analyze
    parameters.allow_changing = ALLOW_CHANGING

    # Set a specific prior distribution instead of the default (Uniform)
    parameters.set_prior("a_snow", spotpy.parameter.Normal(mean=4, stddev=2))

    return socont, parameters, forcing, obs


def build_models():
    """Picklable factory that rebuilds (model, forcing, obs) inside each worker.

    Must be a module-level function so SPOTPY's ``parallel='mpc'`` backend can
    pickle it and ship it to the worker processes.
    """
    socont, _, forcing, obs = build_all()
    return socont, forcing, obs


if __name__ == "__main__":
    # Build once in the main process to obtain the (picklable) parameters used by
    # the sampler. The heavy, C++-backed model/forcing are rebuilt per worker via
    # the factory, so the SpotpySetup itself stays picklable.
    _, parameters, _, _ = build_all()

    # Setup SPOTPY via the factory so the calibration can run in parallel.
    spot_setup = trainer.SpotpySetup.from_factory(
        build_models,
        parameters,
        warmup=365,
        obj_func=spotpy.objectivefunctions.nashsutcliffe,
    )

    # Select number of runs and run spotpy. Set parallel='mpc' to spread the
    # independent Monte Carlo runs across all CPU cores (requires the 'pathos'
    # package); use parallel='seq' for a single-process run. Independent samplers
    # such as 'mc'/'lhs' scale near-linearly with the number of cores.
    nb_runs = 10000
    sampler = trainer.calibrate(
        spot_setup,
        "mc",
        nb_runs,
        dbname="spotpy_socont_sitter_MC",
        dbformat="csv",
        parallel="mpc",
        save_sim=False,
    )

    # Load the results
    results = sampler.getdata()

    # Plot parameter interaction
    spotpy.analyser.plot_parameterInteraction(results)
    plt.tight_layout()
    plt.show()

    # Plot posterior parameter distribution
    posterior = spotpy.analyser.get_posterior(results, percentage=10)
    spotpy.analyser.plot_parameterInteraction(posterior)
    plt.tight_layout()
    plt.show()
