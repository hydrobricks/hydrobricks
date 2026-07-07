import logging
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import spotpy

import hydrobricks as hb
import hydrobricks.trainer as trainer

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
    The whole setup is declared in the project file.
    """
    project = hb.load_project(Path(__file__).parent / "sitter_calibration_project.yaml")

    # Select the parameters to optimize/analyze
    project.parameters.allow_changing = [
        "a_snow",
        "k_quick",
        "A",
        "k_slow_1",
        "percol",
        "k_slow_2",
        "rain_corr_factor",
        "snow_corr_factor",
    ]

    # Set a specific prior distribution instead of the default (Uniform)
    project.parameters.set_prior("a_snow", spotpy.parameter.Normal(mean=4, stddev=2))

    return project.model, project.parameters, project.forcing, project.observations


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
