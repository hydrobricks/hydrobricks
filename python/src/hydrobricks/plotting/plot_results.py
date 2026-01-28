import logging
import warnings

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.colors import LightSource, ListedColormap

from hydrobricks import rxr
from hydrobricks.results import Results

logger = logging.getLogger(__name__)


def plot_hydrograph(
        obs: np.ndarray,
        sim: np.ndarray,
        time: np.ndarray,
        year: int | None = None
) -> None:
    """
    Plot the hydrograph of observed and simulated data.

    Creates a plot comparing observed discharge with simulated discharge over time.
    Optionally filters to a specific year.

    Parameters
    ----------
    obs
        The observed discharge data (1D array).
    sim
        The simulated discharge data (1D array).
    time
        The time array with datetime values.
    year
        The year to plot. If None, plots the entire time series. Default: None
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


def plot_map_hydro_unit_value(
        results: Results,
        unit_ids_raster_path: str,
        component: str,
        date: str,
        dem_path: str | None = None,
        min_val: int = 0,
        max_val: int | None = None,
        figsize: tuple[int, int] = (6.4, 4.8),
        title: str | None = None
) -> None:
    """
    Plot the values of a component at the hydro units on a map.

    Displays spatial distribution of model component values across hydro units
    on a raster map. Optionally overlays a digital elevation model for context.

    Parameters
    ----------
    results
        The results object from a model run.
    unit_ids_raster_path
        Path to the raster file containing hydro unit IDs.
    component
        Name of the component to visualize (e.g., 'discharge', 'storage').
    date
        Date to plot (format: 'YYYY-MM-DD').
    dem_path
        Path to digital elevation model (DEM) raster. If provided, DEM is shown
        as shaded relief background. Default: None
    min_val
        Minimum value for color scale. Default: 0
    max_val
        Maximum value for color scale. If None, auto-scaled. Default: None
    figsize
        Figure size in inches (width, height). Default: (6.4, 4.8)
    title
        Title for the plot. If None, uses the date. Default: None
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
    plt.figure(figsize=figsize)
    if dem_path is not None:
        plt.imshow(shaded_dem, cmap='gray')
        plt.imshow(val_raster, cmap=cmap, alpha=0.7, vmin=min_val, vmax=max_val)
    else:
        plt.imshow(val_raster, cmap=cmap, vmin=min_val, vmax=max_val)
    plt.colorbar(shrink=0.8)
    plt.contour(boundary, levels=[0.5], linewidths=1, colors='black', alpha=1.0)
    plt.axis('off')
    plt.title(title if title else f"{date}")
    plt.tight_layout()
    plt.show()


def create_animated_map_hydro_unit_value(
        results: Results,
        unit_ids_raster_path: str,
        component: str,
        start_date: str,
        end_date: str,
        save_path: str,
        dem_path: str | None = None,
        min_val: int = 0,
        max_val: int | None = None,
        fps: int = 5,
        figsize: tuple[int, int] = (6.4, 4.8)
) -> None:
    """
    Create an animated map of the values of a component at the hydro units.

    Generates a time-series animation showing spatial evolution of a model
    component across hydro units over a specified time period. Saves as GIF.

    Parameters
    ----------
    results
        The results object from a model run.
    unit_ids_raster_path
        Path to the raster file containing hydro unit IDs.
    component
        Name of the component to visualize (e.g., 'discharge', 'storage').
    start_date
        Start date of animation period (format: 'YYYY-MM-DD').
    end_date
        End date of animation period (format: 'YYYY-MM-DD').
    save_path
        Directory path where the animation GIF will be saved.
    dem_path
        Path to digital elevation model (DEM) raster. If provided, DEM is shown
        as shaded relief background. Default: None
    min_val
        Minimum value for color scale. Default: 0
    max_val
        Maximum value for color scale. If None, auto-scaled. Default: None
    fps
        Frames per second for animation. Default: 5
    figsize
        Figure size in inches (width, height). Default: (6.4, 4.8)
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
    fig, ax = plt.subplots(figsize=figsize)

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
    def update(frame: int) -> list:
        """
        Update animation frame with new data and title.

        Parameters
        ----------
        frame
            Current frame number in the animation sequence.

        Returns
        -------
        list
            List of artists to update (image and title).
        """
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
    logger.info(f"Animation saved at {output_path}")


def _generate_cmap_blue() -> ListedColormap:
    """
    Generate a custom blue colormap with white for zero values.

    Creates a YlGnBu (Yellow-Green-Blue) colormap and modifies it to use
    white color for zero values, which is useful for visualizing data where
    zero represents "no data" or "outside catchment".

    Returns
    -------
    ListedColormap
        A custom colormap with white for zero values and blue scale for data.
    """
    # Use the blue colormap
    cmap = plt.get_cmap('YlGnBu')

    # Create a new colormap from the existing one
    new_colors = cmap(np.linspace(0, 1, 256))
    new_colors[0, :] = np.array([1, 1, 1, 1])  # Set the color for 0 values to white
    new_cmap = ListedColormap(new_colors)

    return new_cmap


def _load_units_ids_raster(unit_ids_raster_path: str) -> np.ndarray:
    """
    Load hydro unit IDs from a raster file.

    Reads a raster file containing hydro unit identifiers, suppresses warnings,
    and returns the data as a numpy array.

    Parameters
    ----------
    unit_ids_raster_path
        Path to the raster file containing hydro unit IDs.

    Returns
    -------
    np.ndarray
        2D array of hydro unit IDs matching the raster spatial extent.
    """
    with warnings.catch_warnings():
        warnings.filterwarnings("ignore", category=UserWarning)
        unit_ids_raster = rxr.open_rasterio(unit_ids_raster_path)
        unit_ids_raster = unit_ids_raster.squeeze().drop_vars("band")

    return unit_ids_raster.to_numpy()


def _load_dem(dem_path: str) -> np.ndarray:
    """
    Load digital elevation model (DEM) from a raster file.

    Reads a raster file containing elevation data, suppresses warnings,
    and returns the data as a numpy array.

    Parameters
    ----------
    dem_path
        Path to the raster file containing DEM data.

    Returns
    -------
    np.ndarray
        2D array of elevation values matching the raster spatial extent.
    """
    with warnings.catch_warnings():
        warnings.filterwarnings("ignore", category=UserWarning)
        dem = rxr.open_rasterio(dem_path)
        dem = dem.squeeze().drop_vars("band")

    return dem


def _create_catchment_boundary(unit_ids_raster: np.ndarray) -> np.ndarray:
    """
    Create a binary boundary map of the catchment extent.

    Identifies the edges between catchment area (non-zero hydro unit IDs) and
    non-catchment area (zero values) using directional differences. Creates a
    boundary representation suitable for plotting.

    Parameters
    ----------
    unit_ids_raster
        2D array of hydro unit IDs where 0 represents outside the catchment
        and non-zero values represent hydro unit identifiers.

    Returns
    -------
    np.ndarray
        2D binary array with 1 values at catchment boundaries and 0 elsewhere.
    """
    boundary = np.zeros_like(unit_ids_raster)
    catchment_extent = np.zeros_like(unit_ids_raster)
    catchment_extent[unit_ids_raster != 0] = 1
    boundary[1:, :] = catchment_extent[:-1, :] != catchment_extent[1:, :]
    boundary[:, 1:] |= catchment_extent[:, :-1] != catchment_extent[:, 1:]

    return boundary
