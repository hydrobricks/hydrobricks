import numpy as np
import pandas as pd

import hydrobricks as hb


class Catchment:
    """Creation of catchment-related data"""

    def __init__(self, outline=None):
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")
        if not hb.has_geopandas:
            raise ImportError("geopandas is required to do this.")
        if not hb.has_shapely:
            raise ImportError("shapely is required to do this.")
        self.crs = None
        self.outline = None
        self.dem = None
        self.masked_dem_data = None
        self.unit_ids = None
        self._extract_outline(outline)

    def __del__(self):
        if self.dem is not None:
            self.dem.close()

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
            src = hb.rasterio.open(dem_path)
            self._check_crs(src)
            self.dem = src
            self.masked_dem_data, _ = hb.mask(src, geoms, crop=True)
            self.masked_dem_data[self.masked_dem_data == src.nodata] = np.nan
            if len(self.masked_dem_data.shape) == 3:
                self.masked_dem_data = self.masked_dem_data[0]
            return True
        except ValueError as e:
            print(e)
            return False
        except Exception as e:
            print(e)
            return False

    def create_elevation_bands(self, method='isohypse', number=100, distance=50,
                               min_elevation=None, max_elevation=None):
        """
        Construction of the elevation bands based on the chosen method.

        Parameters
        ----------
        method : str
            The method to build the elevation bands:
            'isohypse' = fixed contour intervals (provide the 'distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'number' parameter)
        number : int, optional
            Number of bands to create when using the 'quantiles' method.
        distance : int, optional
            Distance (m) between the contour lines when using the 'isohypse' method.
        min_elevation : int, optional
            Minimum elevation of the elevation bands (to homogenize between runs).
        max_elevation : int, optional
            Maximum elevation of the elevation bands (to homogenize between runs).

        Returns
        -------
        A dataframe with the elevation bands.
        """
        if method == 'isohypse':
            if min_elevation is None:
                min_elevation = np.nanmin(self.masked_dem_data)
                min_elevation = min_elevation - (min_elevation % distance)
            if max_elevation is None:
                max_elevation = np.nanmax(self.masked_dem_data)
                max_elevation = max_elevation + (distance - max_elevation % distance)
            elevations = np.arange(min_elevation, max_elevation + distance, distance)
        elif method == 'quantiles':
            elevations = np.nanquantile(self.masked_dem_data,
                                        np.linspace(0, 1, num=number))
        else:
            raise ValueError

        self.unit_ids = np.zeros(self.masked_dem_data.shape)
        res_bands = np.zeros(len(elevations) - 1)
        res_elevations = np.zeros(len(elevations) - 1)
        res_elevations_min = np.zeros(len(elevations) - 1)
        res_elevations_max = np.zeros(len(elevations) - 1)
        for i in range(len(elevations) - 1):
            mask_unit = np.logical_and(self.masked_dem_data >= elevations[i],
                                       self.masked_dem_data < elevations[i + 1])
            # Check that all cells in unit_ids are 0
            assert np.count_nonzero(self.unit_ids[mask_unit]) == 0
            # Set the unit id
            self.unit_ids[mask_unit] = i + 1
            # Compute the area of the unit
            n_cells = np.count_nonzero(mask_unit)
            res_bands[i] = round(n_cells * self.dem.res[0] * self.dem.res[1], 2)
            # Compute the mean elevation of the unit
            res_elevations[i] = round(float(np.mean(elevations[i:i + 2])), 2)
            res_elevations_min[i] = round(float(elevations[i]), 2)
            res_elevations_max[i] = round(float(elevations[i + 1]), 2)

        self.unit_ids = self.unit_ids.astype(hb.rasterio.uint16)

        df = pd.DataFrame(columns=['elevation', 'elevation_min',
                                   'elevation_max', 'area'])
        df['elevation'] = res_elevations
        df['elevation_min'] = res_elevations_min
        df['elevation_max'] = res_elevations_max
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

    def save_unit_ids_raster(self, path):
        """
        Save the unit ids raster to a file.

        Parameters
        ----------
        path : str|Path
            Path to the output file.
        """
        if self.unit_ids is None:
            raise ValueError("No unit ids raster to save.")

        # Create the profile
        profile = self.dem.profile
        profile.update(
            dtype=hb.rasterio.uint16,
            count=1,
            compress='lzw',
            nodata=0)

        with hb.rasterio.open(path, 'w', **profile) as dst:
            dst.write(self.unit_ids, 1)

    def _extract_outline(self, outline):
        if not outline:
            return
        shapefile = hb.gpd.read_file(outline)
        self._check_crs(shapefile)
        geoms = shapefile.geometry.values
        self.outline = geoms[0]

    def _check_crs(self, data):
        data_crs = self._get_crs_from_file(data)
        if self.crs is None:
            self.crs = data_crs
        else:
            if self.crs != data_crs:
                raise ValueError("The CRS of the data does not match the CRS of the "
                                 "catchment.")

    @staticmethod
    def _get_crs_from_file(data):
        if isinstance(data, hb.rasterio.DatasetReader):
            return data.crs
        elif isinstance(data, hb.gpd.GeoDataFrame):
            return data.crs
        else:
            raise ValueError("Unknown data format.")
