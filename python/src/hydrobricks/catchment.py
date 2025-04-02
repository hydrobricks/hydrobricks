from pathlib import Path

import numpy as np

import hydrobricks as hb
from hydrobricks.catchment_components.connectivity import Connectivity
from hydrobricks.catchment_components.discretization import Discretization
from hydrobricks.catchment_components.solar_radiation import SolarRadiation
from hydrobricks.catchment_components.topography import Topography

if hb.has_shapely:
    from shapely.geometry import mapping

if hb.has_rasterio:
    from rasterio.mask import mask


class Catchment:
    """
    Creation of catchment-related data

    Parameters
    ----------
    outline : str|Path
        Path to the outline of the catchment.
    land_cover_types : list, optional
        The land cover types of the catchment.
    land_cover_names : list, optional
        The land cover names of the catchment.
    hydro_units_data : pd.DataFrame, optional
        The hydro units data of the catchment.

    Attributes
    ----------
    area : float
        The area of the catchment.
    crs : str
        The crs of the catchment outline.
    outline : shapely.geometry.Polygon
        The outline of the catchment.
    dem : rasterio.DatasetReader
        The DEM of the catchment [m].
    masked_dem_data : np.ndarray
        The masked DEM data of the catchment.
    slope : np.ndarray
        The slope map of the catchment [degrees].
    aspect : np.ndarray
        The aspect map of the catchment.
    map_unit_ids : np.ndarray
        The unit ids as a numpy array matching the DEM extent.
    hydro_units : HydroUnits
        The hydro units of the catchment.
    """

    def __init__(self, outline=None, land_cover_types=None, land_cover_names=None,
                 hydro_units_data=None):
        # Check that the required packages are installed
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")
        if not hb.has_geopandas:
            raise ImportError("geopandas is required to do this.")
        if not hb.has_shapely:
            raise ImportError("shapely is required to do this.")

        # Check that the outline file exists if provided
        if outline is not None and not Path(outline).is_file():
            raise FileNotFoundError(f"File {outline} does not exist.")

        self.area = None
        self.crs = None
        self.outline = None
        self.dem = None
        self.masked_dem_data = None
        self.map_unit_ids = None
        self.hydro_units = hb.HydroUnits(land_cover_types, land_cover_names,
                                         hydro_units_data)

        self._extract_outline(outline)
        self._extract_area(outline)

        self.topography = Topography(self)
        self.discretization = Discretization(self)
        self.connectivity = Connectivity(self)
        self.solar_radiation = SolarRadiation(self)

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
        if not Path(dem_path).is_file():
            raise FileNotFoundError(f"File {dem_path} does not exist.")

        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")
        if not hb.has_shapely:
            raise ImportError("shapely is required to do this.")

        try:
            src = hb.rasterio.open(dem_path)
            self._check_crs(src)
            self.dem = src

            if self.outline is not None:
                geoms = [mapping(polygon) for polygon in self.outline]
                self.masked_dem_data, _ = mask(src, geoms, crop=False)
            else:
                self.masked_dem_data = src.read(1)

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

    def get_hydro_units_nb(self):
        """
        Get the number of hydro units.

        Returns
        -------
        The number of hydro units.
        """
        if self.hydro_units is None:
            raise ValueError("No hydro units to count.")

        return len(self.hydro_units.hydro_units)

    def get_hydro_units_elevations(self):
        """
        Get the elevation of the hydro units.

        Returns
        -------
        The elevation of the hydro units.
        """
        if self.hydro_units is None:
            raise ValueError("No hydro units to count.")

        return self.hydro_units.hydro_units.elevation

    def get_hydro_units_attributes(self):
        """
        Extract the hydro units attributes.

        Returns
        -------
        The hydro units attributes.
        """
        if not hb.has_pyproj:
            raise ImportError("pyproj is required to do this.")

        if self.topography.slope is None or self.topography.aspect is None:
            self.topography.calculate_slope_aspect()

        unit_ids = np.unique(self.map_unit_ids)
        unit_ids = unit_ids[unit_ids != 0]

        if 'id' not in self.hydro_units.hydro_units.columns:
            self.hydro_units.add_property(('id', '-'), unit_ids, set_first=True)

        res_area = []
        res_elevation = []
        res_elevation_mean = []
        res_elevation_min = []
        res_elevation_max = []
        res_slope = []
        res_aspect = []
        res_lat = []
        res_lon = []

        for _, unit_id in enumerate(unit_ids):
            mask_unit = self.map_unit_ids == unit_id

            # Compute the area of the unit
            n_cells = np.count_nonzero(mask_unit)
            res_area.append(round(n_cells * self.dem.res[0] * self.dem.res[1], 2))

            # Compute the mean elevation of the unit
            res_elevation_mean.append(self.topography.extract_unit_mean_elevation(mask_unit))
            if not self.hydro_units.has('elevation'):
                res_elevation.append(self.topography.extract_unit_mean_elevation(mask_unit))
                res_elevation_min.append(self.topography.extract_unit_min_elevation(mask_unit))
                res_elevation_max.append(self.topography.extract_unit_max_elevation(mask_unit))

            # Compute the slope and aspect of the unit
            res_slope.append(self.topography.extract_unit_mean_slope(mask_unit))
            res_aspect.append(self.topography.extract_unit_mean_aspect(mask_unit))

            # Calculate the mean unit coordinates in lat/lon
            lat, lon = self.extract_unit_mean_lat_lon(mask_unit)
            res_lat.append(lat)
            res_lon.append(lon)

        if not self.hydro_units.has('elevation'):
            self.hydro_units.add_property(('elevation', 'm'), res_elevation)
            self.hydro_units.add_property(('elevation_min', 'm'), res_elevation_min)
            self.hydro_units.add_property(('elevation_max', 'm'), res_elevation_max)

        self.hydro_units.add_property(('elevation_mean', 'm'), res_elevation_mean)
        self.hydro_units.add_property(('area', 'm2'), res_area)
        self.hydro_units.add_property(('slope', 'deg'), res_slope)
        self.hydro_units.add_property(('aspect', 'deg'), res_aspect)
        self.hydro_units.add_property(('latitude', 'deg'), res_lat)
        self.hydro_units.add_property(('longitude', 'deg'), res_lon)

        self.hydro_units.check_land_cover_fractions_not_empty()

        self.area = sum(res_area)

        return self.hydro_units

    def save_hydro_units_to_csv(self, path):
        """
        Save the hydro units to a csv file.

        Parameters
        ----------
        path : str|Path
            Path to the output file.
        """
        if self.hydro_units is None:
            raise ValueError("No hydro units to save.")

        self.hydro_units.save_to_csv(path)

    def save_unit_ids_raster(self, output_path, output_filename='unit_ids.tif'):
        """
        Save the unit ids raster to a file.

        Parameters
        ----------
        output_path : str|Path
            Path to the output file.
        output_filename : str, optional
            Name of the output file. Default is 'unit_ids.tif'.
        """
        if self.map_unit_ids is None:
            raise ValueError("No unit ids raster to save.")

        full_path = Path(output_path) / output_filename

        # Create the profile
        profile = self.dem.profile
        profile.update(
            dtype=hb.rasterio.uint16,
            count=1,
            compress='lzw',
            nodata=0)

        with hb.rasterio.open(full_path, 'w', **profile) as dst:
            dst.write(self.map_unit_ids, 1)

    def load_unit_ids_from_raster(self, dir_path, filename='unit_ids.tif'):
        """
        Load hydro units from a raster file.

        Parameters
        ----------
        dir_path : str
            Path to the directory containing the raster file.
        filename : str, optional
            Name of the raster file. Default is 'unit_ids.tif'.
        """
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")

        full_path = Path(dir_path) / filename
        with hb.rasterio.open(full_path) as src:
            self._check_crs(src)
            if self.outline is not None:
                geoms = [mapping(polygon) for polygon in self.outline]
                self.map_unit_ids, _ = mask(src, geoms, crop=False)
            else:
                self.map_unit_ids = src.read(1)
            self.map_unit_ids[self.map_unit_ids == src.nodata] = 0

            if len(self.map_unit_ids.shape) == 3:
                self.map_unit_ids = self.map_unit_ids[0]

            self.map_unit_ids = self.map_unit_ids.astype(hb.rasterio.uint16)

    def get_dem_x_resolution(self):
        """
        Get the DEM x resolution.

        Returns
        -------
        The DEM x resolution.
        """
        return abs(self.dem.transform[0])

    def get_dem_y_resolution(self):
        """
        Get the DEM y resolution.

        Returns
        -------
        The DEM y resolution.
        """
        return abs(self.dem.transform[4])

    def get_dem_pixel_area(self):
        """
        Get the DEM pixel area.

        Returns
        -------
        The DEM pixel area.
        """
        return self.get_dem_x_resolution() * self.get_dem_y_resolution()

    def get_dem_mean_lat_lon(self):
        # Central coordinates of the catchment
        mean_x = self.dem.bounds[0] + (self.dem.bounds[2] - self.dem.bounds[0]) / 2
        if self.dem.bounds[3] > self.dem.bounds[1]:
            mean_y = self.dem.bounds[1] + (self.dem.bounds[3] - self.dem.bounds[1]) / 2
        else:
            mean_y = self.dem.bounds[3] + (self.dem.bounds[1] - self.dem.bounds[3]) / 2

        # Get the mean coordinates of the unit in lat/lon
        transformer = hb.pyproj.Transformer.from_crs(self.crs, 4326, always_xy=True)
        mean_lon, mean_lat = transformer.transform(mean_x, mean_y)

        return mean_lat, mean_lon

    def create_dem_pixel_geometry(self, i, j):
        """
        Create a shapely geometry of the DEM pixel.

        Parameters
        ----------
        i : int
            The row of the pixel.
        j : int
            The column of the pixel.

        Returns
        -------
        The shapely geometry of the pixel.
        """
        if not hb.has_shapely:
            raise ImportError("shapely is required to do this.")

        xy = self.dem.xy(i, j)
        x_size = self.get_dem_x_resolution()
        y_size = self.get_dem_y_resolution()
        x_min = xy[0] - x_size / 2
        y_min = xy[1] - y_size / 2
        x_max = xy[0] + x_size / 2
        y_max = xy[1] + y_size / 2

        return hb.shapely.geometry.box(x_min, y_min, x_max, y_max)

    def initialize_area_from_land_cover_change(self, land_cover_name,
                                               land_cover_change):
        """
        Initialize the HydroUnits cover area from a land cover change object.

        Parameters
        ----------
        land_cover_name : str
            The name of the land cover to initialize.
        land_cover_change : pd.DataFrame
            The land cover change dataframe.
        """
        if self.map_unit_ids is None:
            raise ValueError("No hydro units to initialize.")

        if land_cover_name == 'ground':
            raise ValueError("You should not initialize the 'ground' land cover type.")

        self.hydro_units.initialize_from_land_cover_change(
            land_cover_name, land_cover_change)

    def initialize_land_cover_fractions(self):
        self.hydro_units.initialize_land_cover_fractions()

    def discretize_by(self, *args, **kwargs):
        """
        Call the discretize_by method of the Discretization class.
        """
        return self.discretization.discretize_by(*args, **kwargs)

    def create_elevation_bands(self, *args, **kwargs):
        """
        Call the create_elevation_bands method of the Discretization class.
        """
        return self.discretization.create_elevation_bands(*args, **kwargs)

    def get_mean_elevation(self):
        """
        Call the get_mean_elevation method of the Topography class.
        """
        return self.topography.get_mean_elevation()

    def resample_dem_and_calculate_slope_aspect(self, *args, **kwargs):
        """
        Call the resample_dem_and_calculate_slope_aspect method of the Topography class.
        """
        return self.topography.resample_dem_and_calculate_slope_aspect(*args, **kwargs)

    def calculate_slope_aspect(self):
        """
        Call the calculate_slope_aspect method of the Topography class.
        """
        return self.topography.calculate_slope_aspect()

    def get_hillshade(self, *args, **kwargs):
        """
        Call the get_hillshade method of the Topography class.
        """
        return self.topography.get_hillshade(*args, **kwargs)

    def calculate_connectivity(self, *args, **kwargs):
        """
        Call the calculate_connectivity method of the Connectivity class.
        """
        return self.connectivity.calculate(*args, **kwargs)

    def calculate_daily_potential_radiation(self, *args, **kwargs):
        """
        Call the calculate_daily_potential_radiation method of the SolarRadiation class.
        """
        return self.solar_radiation.calculate_daily_potential_radiation(*args, **kwargs)

    @staticmethod
    def calculate_cast_shadows(*args, **kwargs):
        """
        Call the calculate_cast_shadows method of the SolarRadiation class.
        """
        return SolarRadiation.calculate_cast_shadows(*args, **kwargs)

    def load_mean_annual_radiation_raster(self, *args, **kwargs):
        """
        Call the load_mean_annual_radiation_raster method of the SolarRadiation class.
        """
        return self.solar_radiation.load_mean_annual_radiation_raster(*args, **kwargs)

    def upscale_and_save_mean_annual_radiation_rasters(self, *args, **kwargs):
        """
        Call the upscale_and_save_mean_annual_radiation_rasters method of the SolarRadiation class.
        """
        return self.solar_radiation.upscale_and_save_mean_annual_radiation_rasters(*args, **kwargs)

    @staticmethod
    def get_solar_declination_rad(*args, **kwargs):
        """
        Call the get_solar_declination_rad method of the SolarRadiation class.
        """
        return SolarRadiation.get_solar_declination_rad(*args, **kwargs)

    @staticmethod
    def get_solar_hour_angle_limit(*args, **kwargs):
        """
        Call the get_solar_hour_angle_limit method of the SolarRadiation class.
        """
        return SolarRadiation.get_solar_hour_angle_limit(*args, **kwargs)

    @staticmethod
    def get_solar_zenith(*args, **kwargs):
        """
        Call the get_solar_zenith method of the SolarRadiation class.
        """
        return SolarRadiation.get_solar_zenith(*args, **kwargs)

    @staticmethod
    def get_solar_azimuth_to_south(*args, **kwargs):
        """
        Call the get_solar_azimuth_to_south method of the SolarRadiation class.
        """
        return SolarRadiation.get_solar_azimuth_to_south(*args, **kwargs)

    @staticmethod
    def get_solar_azimuth_to_north(*args, **kwargs):
        """
        Call the get_solar_azimuth_to_north method of the SolarRadiation class.
        """
        return SolarRadiation.get_solar_azimuth_to_north(*args, **kwargs)

    def extract_unit_mean_lat_lon(self, mask_unit):
        # Get rows and cols of the unit
        rows, cols = np.where(mask_unit)

        # Get coordinates of the unit
        xs, ys = hb.rasterio.transform.xy(self.dem.transform, list(rows), list(cols))

        # Get the mean coordinates of the unit
        mean_x = np.nanmean(xs)
        mean_y = np.nanmean(ys)

        # Get the mean coordinates of the unit in lat/lon
        transformer = hb.pyproj.Transformer.from_crs(self.crs, 4326, always_xy=True)
        mean_lon, mean_lat = transformer.transform(mean_x, mean_y)

        return mean_lat, mean_lon

    def _extract_area(self, outline):
        # The outline has to be given in meters.
        if not outline:
            return
        shapefile = hb.gpd.read_file(outline)
        self._check_crs(shapefile)
        area = hb.utils.compute_area(shapefile)
        self.area = area

    def _extract_outline(self, outline):
        if not outline:
            return
        shapefile = hb.gpd.read_file(outline)
        self._check_crs(shapefile)
        geoms = shapefile.geometry.values
        self.outline = geoms

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

    # Copy the docstring from attached classes
    discretize_by.__doc__ = Discretization.discretize_by.__doc__
    create_elevation_bands.__doc__ = Discretization.create_elevation_bands.__doc__
    get_mean_elevation.__doc__ = Topography.get_mean_elevation.__doc__
    resample_dem_and_calculate_slope_aspect.__doc__ = Topography.resample_dem_and_calculate_slope_aspect.__doc__
    calculate_slope_aspect.__doc__ = Topography.calculate_slope_aspect.__doc__
    get_hillshade.__doc__ = Topography.get_hillshade.__doc__
    calculate_connectivity.__doc__ = Connectivity.calculate.__doc__
    calculate_daily_potential_radiation.__doc__ = SolarRadiation.calculate_daily_potential_radiation.__doc__
    calculate_cast_shadows.__doc__ = SolarRadiation.calculate_cast_shadows.__doc__
    load_mean_annual_radiation_raster.__doc__ = SolarRadiation.load_mean_annual_radiation_raster.__doc__
    upscale_and_save_mean_annual_radiation_rasters.__doc__ = SolarRadiation.upscale_and_save_mean_annual_radiation_rasters.__doc__
    get_solar_declination_rad.__doc__ = SolarRadiation.get_solar_declination_rad.__doc__
    get_solar_hour_angle_limit.__doc__ = SolarRadiation.get_solar_hour_angle_limit.__doc__
    get_solar_zenith.__doc__ = SolarRadiation.get_solar_zenith.__doc__
    get_solar_azimuth_to_south.__doc__ = SolarRadiation.get_solar_azimuth_to_south.__doc__
    get_solar_azimuth_to_north.__doc__ = SolarRadiation.get_solar_azimuth_to_north.__doc__