import warnings

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.colors import LightSource, ListedColormap

import hydrobricks as hb


def plot_hydrograph(obs, sim, time, year=None):
    """
    Plot the hydrograph of observed and simulated data.

    Parameters
    ----------
    obs : array-like
        The observed data.
    sim : array-like
        The simulated data.
    time : array-like
        The time array.
    year : int (optional)
        The year to plot (default: None).
    """
    plt.figure()
    if year is not None:
        dates = time[time.dt.year == year]
        plt.plot(dates, obs[dates.index], label='observed', color='black')
        plt.plot(dates, sim[dates.index], label='model', color='blue')
    else:
        plt.plot(time, obs, label='observed', color='black')
        plt.plot(time, sim, label='model', color='blue')
    plt.legend()
    plt.ylabel('discharge (mm/day)')
    plt.ylim(0, None)
    plt.tight_layout()
    plt.show()


def plot_map_hydro_unit_value(results, unit_ids_raster_path, component, date,
                              dem_path=None, min_val=0, max_val=None):
    """
    Plot the values of a component at the hydro units on a map.

    Parameters
    ----------
    results : Results
        The results of the model run.
    unit_ids_raster_path : str
        The path to the raster file with the hydro units ids.
    component : str
        The name of the component.
    date : str
        The date of the values to plot (format: 'YYYY-MM-DD').
    dem_path : str (optional)
        The path to the digital elevation model (default: None). If provided,
        the DEM will be used to create a shaded relief map.
    min_val : float (optional)
        The minimum value for the color scale (default: 0).
    max_val : float (optional)
        The maximum value for the color scale (default: None).
    """
    data = results.get_hydro_units_values(component, date)
    hydro_units_ids = results.hydro_units_ids
    unit_ids_raster = _load_units_ids_raster(unit_ids_raster_path)

    # Create a new raster with the values of the component at the hydro units
    val_raster = np.zeros(unit_ids_raster.shape)
    for i, unit_id in enumerate(hydro_units_ids):
        val_raster[unit_ids_raster == unit_id] = data[i]

    # Find the boundary between non-zero values and zeros
    boundary = _create_catchment_boundary(unit_ids_raster)

    # Load the DEM and create a shaded relief map
    shaded_dem = None
    if dem_path is not None:
        dem = _load_dem(dem_path)
        ls = LightSource(azdeg=315, altdeg=45)
        shaded_dem = ls.hillshade(dem.to_numpy(), vert_exag=0.1)

    # Plot
    cmap = _generate_cmap_blue()
    plt.figure()
    if dem_path is not None:
        plt.imshow(shaded_dem, cmap='gray')
        plt.imshow(val_raster, cmap=cmap, alpha=0.7, vmin=min_val, vmax=max_val)
    else:
        plt.imshow(val_raster, cmap=cmap, vmin=min_val, vmax=max_val)
    plt.colorbar(shrink=0.8)
    plt.contour(boundary, levels=[0.5], linewidths=1, colors='black', alpha=1.0)
    plt.axis('off')
    plt.title(f"{date}")
    plt.tight_layout()
    plt.show()


def create_animated_map_hydro_unit_value(results, unit_ids_raster_path, component,
                                         start_date, end_date, save_path, dem_path=None,
                                         min_val=0, max_val=None, fps=5):
    """
    Create an animated map of the values of a component at the hydro units.

    Parameters
    ----------
    results : Results
        The results of the model run.
    unit_ids_raster_path : str
        The path to the raster file with the hydro units ids.
    component : str
        The name of the component.
    start_date : str
        The start date of the period to plot (format: 'YYYY-MM-DD').
    end_date : str
        The end date of the period to plot (format: 'YYYY-MM-DD').
    save_path : str
        The path where to save the animation.
    dem_path : str (optional)
        The path to the digital elevation model (default: None). If provided,
        the DEM will be used to create a shaded relief map.
    min_val : float (optional)
        The minimum value for the color scale (default: 0).
    max_val : float (optional)
        The maximum value for the color scale (default: None).
    fps : int (optional)
        The number of frames per second for the animation (default: 5).
    """
    # Get the data
    data = results.get_hydro_units_values(component, start_date, end_date)
    dates = results.get_time_array(start_date, end_date)
    hydro_units_ids = results.hydro_units_ids
    unit_ids_raster = _load_units_ids_raster(unit_ids_raster_path)

    # Extract dimensions
    len_time = data.shape[1]
    len_x = unit_ids_raster.shape[0]
    len_y = unit_ids_raster.shape[1]

    # Create a new tensor with the values of the component at the hydro units
    data_3d = np.zeros((len_time, len_x, len_y))
    for t in range(len_time):
        for i, unit_id in enumerate(hydro_units_ids):
            data_3d[t, unit_ids_raster == unit_id] = data[i, t]

    # Find the boundary between non-zero values and zeros
    boundary = _create_catchment_boundary(unit_ids_raster)

    # Load the DEM and create a shaded relief map
    shaded_dem = None
    if dem_path is not None:
        dem = _load_dem(dem_path)
        ls = LightSource(azdeg=315, altdeg=45)
        shaded_dem = ls.hillshade(dem.to_numpy(), vert_exag=0.1)

    # Create the animation
    cmap = _generate_cmap_blue()
    fig, ax = plt.subplots()

    # Initialize the plot with the first frame of the data
    if dem_path is not None:
        plt.imshow(shaded_dem, cmap='gray')
        im = ax.imshow(data_3d[0], cmap=cmap, alpha=0.7, vmin=min_val, vmax=max_val)
    else:
        im = ax.imshow(data_3d[0], cmap=cmap, vmin=min_val, vmax=max_val)

    plt.axis('off')

    # Add the catchment boundary
    plt.contour(boundary, levels=[0.5], linewidths=1, colors='black', alpha=1.0)

    # Initialize title
    title = ax.set_title('Frame 0')
    plt.tight_layout()

    # Define the update function to be called for each frame
    def update(frame):
        im.set_array(data_3d[frame])  # Update the image data
        date = np.datetime_as_string(dates[frame], unit='D')
        title.set_text(date)  # Update the title
        return [im, title]

    # Create the animation
    ani = FuncAnimation(fig, update, frames=range(len_time), blit=False)

    # Add colorbar
    cbar = plt.colorbar(im, ax=ax, shrink=0.8)
    cbar.set_label('[mm]')

    # Save the animation
    output_path = f"{save_path}/{component.replace(':', '_')}.gif"
    ani.save(output_path, fps=fps)
    print(f"Animation saved at {output_path}")


def _generate_cmap_blue():
    # Use the blue colormap
    cmap = plt.get_cmap('YlGnBu')

    # Create a new colormap from the existing one
    new_colors = cmap(np.linspace(0, 1, 256))
    new_colors[0, :] = np.array([1, 1, 1, 1])  # Set the color for 0 values to white
    new_cmap = ListedColormap(new_colors)

    return new_cmap


def _load_units_ids_raster(unit_ids_raster_path):
    with warnings.catch_warnings():
        warnings.filterwarnings("ignore", category=UserWarning)
        unit_ids_raster = hb.rxr.open_rasterio(unit_ids_raster_path)
        unit_ids_raster = unit_ids_raster.squeeze().drop_vars("band")

    return unit_ids_raster.to_numpy()


def _load_dem(dem_path):
    with warnings.catch_warnings():
        warnings.filterwarnings("ignore", category=UserWarning)
        dem = hb.rxr.open_rasterio(dem_path)
        dem = dem.squeeze().drop_vars("band")

    return dem


def _create_catchment_boundary(unit_ids_raster):
    boundary = np.zeros_like(unit_ids_raster)
    catchment_extent = np.zeros_like(unit_ids_raster)
    catchment_extent[unit_ids_raster != 0] = 1
    boundary[1:, :] = catchment_extent[:-1, :] != catchment_extent[1:, :]
    boundary[:, 1:] |= catchment_extent[:, :-1] != catchment_extent[:, 1:]

    return boundary
