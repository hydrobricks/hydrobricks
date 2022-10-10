import matplotlib.pyplot as plt


def plot_precip_per_unit(units_precip, hydro_units):
    sum_precip = units_precip.sum(axis=0)

    plt.figure()
    x = hydro_units['elevation'].tolist()
    y = sum_precip.astype('int').tolist()
    plt.plot(x, y)
    plt.xlabel('elevation [m]')
    plt.ylabel('precipitation total [mm]')
    plt.tight_layout()
    plt.show()


def plot_hydro_units_values(results, index, units, units_labels):
    plt.figure()
    legend = []
    for unit in units:
        results.hydro_units_values.loc[index].loc[unit].plot.line(x="time")
        legend.append(units_labels[index] + f"({unit})")
    plt.legend(legend)
    plt.tight_layout()
    plt.show()
