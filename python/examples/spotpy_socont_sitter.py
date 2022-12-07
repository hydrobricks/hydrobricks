import matplotlib.pyplot as plt
import spotpy
import hydrobricks as hb
from setups.socont_sitter import parameters, socont, forcing, obs

# Select the parameters to optimize/analyze
parameters.allow_changing = ['a_snow', 'k_quick', 'A', 'k_slow_1', 'percol', 'k_slow_2',
                             'precip_corr_factor']

spot_setup = hb.SpotpySetup(socont, parameters, forcing, obs, warmup=365,
                            obj_func=spotpy.objectivefunctions.nashsutcliffe,
                            invert_obj_func=True)

sampler = spotpy.algorithms.sceua(spot_setup, dbname='SCEUA_socont', dbformat='csv')

# Select number of maximum repetitions
rep = 1000

sampler.sample(rep)

results = spotpy.analyser.load_csv_results('SCEUA_socont')

fig = plt.figure(1, figsize=(9, 5))
plt.plot(results['like1'])
plt.show()
plt.ylabel('RMSE')
plt.xlabel('Iteration')
plt.show()

bestindex, bestobjf = spotpy.analyser.get_minlikeindex(results)
best_model_run = results[bestindex]
fields = [word for word in best_model_run.dtype.names if word.startswith('sim')]
best_simulation = list(best_model_run[fields])

fig = plt.figure(figsize=(16, 9))
ax = plt.subplot(1, 1, 1)
ax.plot(best_simulation, color='black', linestyle='solid',
        label='Best objf.=' + str(bestobjf))
ax.plot(spot_setup.evaluation(), 'r.', markersize=3, label='Observation data')
plt.xlabel('Number of Observation Points')
plt.ylabel('Discharge [l s-1]')
plt.legend(loc='upper right')
plt.show()

# Cleanup
socont.cleanup()
