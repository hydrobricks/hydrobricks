import numpy as np

from hydrobricks import xr
from hydrobricks._optional import HAS_XARRAY


class Results:
    """
    Class for the detailed results of a model run.
    This class is used to read the results of a model run (from a netCDF file)
    and to provide methods to extract the results.
    """

    def __init__(self, filename: str) -> None:
        """
        Initialize Results instance from a netCDF file.

        Parameters
        ----------
        filename
            Path to the netCDF results file from a model run.

        Raises
        ------
        ImportError
            If xarray is not installed.
        FileNotFoundError
            If the specified file does not exist.
        """
        if not HAS_XARRAY:
            raise ImportError("xarray is required to do this.")
        self.results = xr.open_dataset(filename)
        self.labels_distributed = self.results.attrs.get('labels_distributed')
        self.labels_aggregated = self.results.attrs.get('labels_aggregated')
        self.labels_land_cover = self.results.attrs.get('labels_land_covers')
        self.hydro_units_ids = self.results.hydro_units_ids.to_numpy()

    def __del__(self) -> None:
        """Close the netCDF dataset on deletion."""
        self.results.close()

    def list_hydro_units_components(self) -> None:
        """Print a list of the distributed (hydro unit level) components."""
        print("Hydro units components:")
        if isinstance(self.labels_distributed, str):
            print('- ' + self.labels_distributed)
            return
        elif isinstance(self.labels_distributed, list):
            for label in self.labels_distributed:
                print('- ' + label)

    def list_sub_basin_components(self) -> None:
        """Print a list of the aggregated (sub-basin level) components."""
        print("Sub basins components:")
        if isinstance(self.labels_aggregated, str):
            print('- ' + self.labels_aggregated)
            return
        elif isinstance(self.labels_aggregated, list):
            for label in self.labels_aggregated:
                print('- ' + label)

    def get_land_cover_areas(self, land_cover: str) -> np.ndarray:
        """
        Get the areas of a land cover across the hydro units.

        Parameters
        ----------
        land_cover
            The name of the land cover type.

        Returns
        -------
        np.ndarray
            Areas of the land cover across the hydro units (2D array: time × units).

        Raises
        ------
        ValueError
            If the land cover is not found in the results.
        """
        i_land_cover = self.labels_land_cover.index(land_cover)
        lc_fraction = self.results.land_cover_fractions[i_land_cover, :, :]
        hydro_units_areas = self.results.hydro_units_areas

        return hydro_units_areas * lc_fraction

    def get_hydro_units_values(
            self,
            component: str,
            start_date: str | None = None,
            end_date: str | None = None
    ) -> np.ndarray:
        """
        Get the values of a component at the hydro units.

        Parameters
        ----------
        component
            The name of the component (e.g., 'snowpack', 'soil_moisture').
        start_date
            The start date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns full time series. Default: None
        end_date
            The end date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns up to end of time series. Default: None

        Returns
        -------
        np.ndarray
            Values of the component at the hydro units.
            Shape: (n_timesteps, n_hydro_units) or (n_hydro_units,) if single date.

        Raises
        ------
        ValueError
            If the component is not found in the results.
        """
        i_component = self.labels_distributed.index(component)

        if start_date is None:
            return self.results.hydro_units_values[i_component].to_numpy()

        if end_date is None:
            return self.results.hydro_units_values[i_component].sel(
                time=start_date).to_numpy()

        return self.results.hydro_units_values[i_component].sel(
            time=slice(start_date, end_date)).to_numpy()

    def get_mean_hydro_units_values(
            self,
            land_cover: str,
            component: str,
            start_date: str | None = None,
            end_date: str | None = None
    ) -> np.ndarray:
        """
        Get the mean values of a component across the hydro units weighted by land cover area.

        Parameters
        ----------
        land_cover
            The name of the land cover type to weight by.
        component
            The name of the component (e.g., 'snowpack', 'soil_moisture').
        start_date
            The start date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns full time series. Default: None
        end_date
            The end date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns up to end of time series. Default: None

        Returns
        -------
        np.ndarray
            Weighted mean values of the component across the hydro units (1D time series).

        Raises
        ------
        ValueError
            If the land cover or component is not found in the results.
        """
        values = self.get_hydro_units_values(component, start_date, end_date)
        lc_areas = self.get_land_cover_areas(land_cover)

        return values * lc_areas / lc_areas.sum(axis=0)

    def get_mean_swe(
            self,
            start_date: str | None = None,
            end_date: str | None = None
    ) -> np.ndarray:
        """
        Get the mean snow water equivalent (SWE) across the hydro units weighted by land cover.

        Parameters
        ----------
        start_date
            The start date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns full time series. Default: None
        end_date
            The end date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns up to end of time series. Default: None

        Returns
        -------
        np.ndarray
            Mean SWE across the hydro units (1D time series, units: mm water equivalent).

        Raises
        ------
        ValueError
            If SWE is not found in the component labels.
        """
        lc_swe = []
        lc_areas = []
        for land_cover in self.labels_land_cover:
            swe = self.get_hydro_units_values(
                component=f'{land_cover}_snowpack:snow_content',
                start_date=start_date,
                end_date=end_date
            )
            lc_swe.append(swe)
            lc_areas.append(self.get_land_cover_areas(land_cover))

        lc_swe = np.array(lc_swe)
        lc_areas = np.array(lc_areas)

        # Flatten the first dimension (land covers) into the hydro units dimension
        lc_swe = lc_swe.reshape(-1, lc_swe.shape[2])
        lc_areas = lc_areas.reshape(-1, lc_areas.shape[2])

        total_areas = lc_areas.sum(axis=0)
        mean_swe = (lc_swe * lc_areas).sum(axis=0) / total_areas

        return mean_swe

    def get_time_array(self, start_date: str, end_date: str) -> np.ndarray:
        """
        Get the time array.

        Parameters
        ----------
        start_date
            The start date of the period to extract.
        end_date
            The end date of the period to extract.

        Returns
        -------
        The time array.
        """
        return self.results.time.sel(time=slice(start_date, end_date)).to_numpy()
