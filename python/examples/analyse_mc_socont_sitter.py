import shutil

import matplotlib.pyplot as plt
import spotpy
from setups.socont_sitter import forcing, obs, parameters, socont, tmp_dir

import hydrobricks as hb

# Select the parameters to optimize/analyze
parameters.allow_changing = ['a_snow', 'k_quick', 'A', 'k_slow_1', 'percol', 'k_slow_2',
                             'precip_corr_factor']

# Set a specific prior distribution instead of the default (Uniform)
parameters.set_prior('a_snow', spotpy.parameter.Normal(mean=4, stddev=2))

# Setup SPOTPY
spot_setup = hb.SpotpySetup(socont, parameters, forcing, obs, warmup=365,
                            obj_func=spotpy.objectivefunctions.nashsutcliffe)

# Select number of runs and run spotpy
nb_runs = 10000
sampler = spotpy.algorithms.mc(spot_setup, dbname='socont_sitter_MC', dbformat='csv',
                               save_sim=False)
sampler.sample(nb_runs)

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

# Cleanup
try:
    socont.cleanup()
    shutil.rmtree(tmp_dir)
except Exception:
    print("Failed to clean up.")
