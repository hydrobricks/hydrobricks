import xarray as xr


class Results:
    """
    Class for the detailed results of a model run.
    This class is used to read the results of a model run (from a netCDF file)
    and to provide methods to extract the results.
    """

    def __init__(self, filename):
        self.results = xr.open_dataset(filename)
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

    def get_hydro_units_values(self, component, start_date, end_date=None):
        """
        Get the values of a component at the hydro units.

        Parameters
        ----------
        component : str
            The name of the component.
        start_date : str
            The start date of the period to extract.
        end_date : str (optional)
            The end date of the period to extract (default: None).

        Returns
        -------
        The values of the component at the hydro units.
        """
        i_component = self.labels_distributed.index(component)

        if end_date is None:
            return self.results.hydro_units_values[i_component].sel(
                time=start_date).to_numpy()

        return self.results.hydro_units_values[i_component].sel(
            time=slice(start_date, end_date)).to_numpy()

    def get_time_array(self, start_date, end_date):
        """
        Get the time array.

        Parameters
        ----------
        start_date : str
            The start date of the period to extract.
        end_date : str
            The end date of the period to extract.

        Returns
        -------
        The time array.
        """
        return self.results.time.sel(time=slice(start_date, end_date)).to_numpy()
