import warnings

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.colors import ListedColormap

import hydrobricks as hb


def plot_map_hydro_unit_value(results, unit_ids_raster_path, component, date,
                              min_val=0, max_val=None):
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

    # Plot
    plt.figure()
    plt.imshow(val_raster, cmap=_generate_cmap_blue())
    plt.clim(min_val, max_val)
    plt.colorbar(shrink=0.8)
    plt.contour(boundary, levels=[0.5], linewidths=1, colors='black', alpha=1.0)
    plt.axis('off')
    plt.title(f"{date}")
    plt.tight_layout()
    plt.show()


def create_animated_map_hydro_unit_value(results, unit_ids_raster_path, component,
                                         start_date, end_date, save_path, min_val=0,
                                         max_val=None, fps=5):
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

    # Create the animation
    fig, ax = plt.subplots()

    # Initialize the plot with the first frame of the data
    im = ax.imshow(data_3d[0], cmap=_generate_cmap_blue(), vmin=min_val, vmax=max_val)
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


def _create_catchment_boundary(unit_ids_raster):
    boundary = np.zeros_like(unit_ids_raster)
    catchment_extent = np.zeros_like(unit_ids_raster)
    catchment_extent[unit_ids_raster != 0] = 1
    boundary[1:, :] = catchment_extent[:-1, :] != catchment_extent[1:, :]
    boundary[:, 1:] |= catchment_extent[:, :-1] != catchment_extent[:, 1:]

    return boundary
