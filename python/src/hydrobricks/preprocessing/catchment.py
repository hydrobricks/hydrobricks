import hydrobricks as hb
import numpy as np
import pandas as pd


class Catchment:
    """Creation of catchment-related data"""

    def __init__(self, outline=None):
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")
        if not hb.has_geopandas:
            raise ImportError("geopandas is required to do this.")
        if not hb.has_shapely:
            raise ImportError("shapely is required to do this.")
        self.outline = None
        self.dem = None
        self.masked_dem_data = None
        self._extract_outline(outline)

    def extract_dem(self, dem_path) -> bool:
        """
        Extract the DEM data for the catchment. Does not handle change in coordinates.

        Parameters
        ----------
        dem_path : str|Path
            Path of the DEM file.

        Returns
        -------
        True if successful, False otherwise.
        """
        try:
            geoms = [hb.mapping(self.outline)]
            with hb.rasterio.open(dem_path) as src:
                self.dem = src
                self.masked_dem_data, _ = hb.mask(src, geoms, crop=True)
                self.masked_dem_data[self.masked_dem_data == src.nodata] = np.nan
            return True
        except ValueError as e:
            print(e)
            return False
        except Exception as e:
            print(e)
            return False

    def get_elevation_bands(self, method='isohypse', number=100, distance=50):
        """
        Construction of the elevation bands based on the chosen method.

        Parameters
        ----------
        method : str
            The method to build the elevation bands:
            'isohypse' = fixed contour intervals (provide the 'distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'number' parameter)
        number : int
            Number of bands to create when using the 'quantiles' method.
        distance : int
            Distance (m) between the contour lines when using the 'isohypse' method.

        Returns
        -------
        A dataframe with the elevation bands.
        """
        if method == 'isohypse':
            min_val = np.nanmin(self.masked_dem_data)
            max_val = np.nanmax(self.masked_dem_data)
            elevations = np.arange(min_val, max_val + distance, distance)
        elif method == 'quantiles':
            elevations = np.nanquantile(self.masked_dem_data,
                                        np.linspace(0, 1, num=number))
        else:
            raise ValueError

        res_bands = np.zeros(len(elevations) - 1)
        res_elevations = np.zeros(len(elevations) - 1)
        for i in range(len(elevations) - 1):
            n_cells = np.count_nonzero(
                np.logical_and(self.masked_dem_data >= elevations[i],
                               self.masked_dem_data <= elevations[i + 1]))
            res_bands[i] = round(n_cells * self.dem.res[0] * self.dem.res[1], 2)
            res_elevations[i] = round(float(np.mean(elevations[i:i+2])), 2)

        df = pd.DataFrame(columns=['elevation', 'area'])
        df['elevation'] = res_elevations
        df['area'] = res_bands

        return df

    def get_mean_elevation(self):
        """
        Get the catchment mean elevation.

        Returns
        -------
        The catchment mean elevation.
        """
        return np.nanmean(self.masked_dem_data)

    def _extract_outline(self, outline):
        if not outline:
            return
        shapefile = hb.gpd.read_file(outline)
        geoms = shapefile.geometry.values
        self.outline = geoms[0]
