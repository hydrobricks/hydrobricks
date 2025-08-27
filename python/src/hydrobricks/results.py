import numpy as np

import hydrobricks as hb


class Results:
    """
    Class for the detailed results of a model run.
    This class is used to read the results of a model run (from a netCDF file)
    and to provide methods to extract the results.
    """

    def __init__(self, filename: str):
        if not hb.has_xarray:
            raise ImportError("xarray is required to do this.")
        self.results = hb.xr.open_dataset(filename)
        self.labels_distributed = self.results.attrs.get('labels_distributed')
        self.labels_aggregated = self.results.attrs.get('labels_aggregated')
        self.labels_land_cover = self.results.attrs.get('labels_land_covers')
        self.hydro_units_ids = self.results.hydro_units_ids.to_numpy()

    def __del__(self):
        self.results.close()

    def list_hydro_units_components(self):
        """ Print a list of the components related to the hydro units."""
        print("Hydro units components:")
        if isinstance(self.labels_distributed, str):
            print('- ' + self.labels_distributed)
            return
        elif isinstance(self.labels_distributed, list):
            for label in self.labels_distributed:
                print('- ' + label)

    def list_sub_basin_components(self):
        """ Print a list of the components related to the sub basins."""
        print("Sub basins components:")
        if isinstance(self.labels_aggregated, str):
            print('- ' + self.labels_aggregated)
            return
        elif isinstance(self.labels_aggregated, list):
            for label in self.labels_aggregated:
                print('- ' + label)

    def get_hydro_units_values(
            self,
            component: str,
            start_date: str = None,
            end_date: str = None
    ) -> np.ndarray:
        """
        Get the values of a component at the hydro units.

        Parameters
        ----------
        component
            The name of the component.
        start_date
            The start date of the period to extract.
        end_date
            The end date of the period to extract (default: None).

        Returns
        -------
        The values of the component at the hydro units.
        """
        i_component = self.labels_distributed.index(component)

        if start_date is None:
            return self.results.hydro_units_values[i_component].to_numpy()

        if end_date is None:
            return self.results.hydro_units_values[i_component].sel(
                time=start_date).to_numpy()

        return self.results.hydro_units_values[i_component].sel(
            time=slice(start_date, end_date)).to_numpy()

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
