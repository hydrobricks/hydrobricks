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


def build():
    """Build everything the calibration needs: model, parameters, forcing, obs.

    For parallel runs this factory is shipped to each worker process to rebuild
    the model there, so it must be a module-level function (not a lambda/closure).
    """
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
    parameters.allow_changing = [
        "a_snow",
        "k_quick",
        "A",
        "k_slow_1",
        "percol",
        "k_slow_2",
        "precip_corr_factor",
    ]

    # Set a specific prior distribution instead of the default (Uniform)
    parameters.set_prior("a_snow", spotpy.parameter.Normal(mean=4, stddev=2))

    return socont, parameters, forcing, obs


if __name__ == "__main__":
    # A single call builds the (picklable) setup and runs the sampler. Set
    # parallel='mpc' to spread the independent Monte Carlo runs across CPU cores
    # (requires the 'pathos' package); use parallel='seq' for a single process.
    # Independent samplers such as 'mc'/'lhs' scale near-linearly with the cores.
    # Use n_workers to cap the number of processes (default: all cores).
    sampler = trainer.calibrate_from_factory(
        build,
        "mc",
        10000,
        warmup=365,
        obj_func=spotpy.objectivefunctions.nashsutcliffe,
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
