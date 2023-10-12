import itertools
import math
import warnings

import numpy as np
import pandas as pd

import hydrobricks as hb
from hydrobricks.units import convert_unit, Unit

if hb.has_shapely:
    from shapely.geometry import box, shape
    from shapely.ops import unary_union
    from shapely.geometry import mapping

if hb.has_rasterio:
    from rasterio.mask import mask
    from rasterio.features import geometry_window


class Catchment:
    """Creation of catchment-related data"""

    def __init__(self, outline=None):
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")
        if not hb.has_geopandas:
            raise ImportError("geopandas is required to do this.")
        if not hb.has_shapely:
            raise ImportError("shapely is required to do this.")
        self.area = None
        self.crs = None
        self.outline = None
        self.dem = None
        self.masked_dem_data = None
        self.slope = None
        self.aspect = None
        self.map_unit_ids = None
        self.hydro_units = hb.HydroUnits()
        self._extract_outline(outline)
        self._extract_area(outline)

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
            geoms = [mapping(self.outline)]
            src = hb.rasterio.open(dem_path)
            self._check_crs(src)
            self.dem = src
            self.masked_dem_data, _ = mask(src, geoms, crop=False)
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
        """
        self.discretize_by('elevation', method, number, distance,
                           min_elevation, max_elevation)

    def discretize_by(self, criteria, elevation_method='isohypse', elevation_number=100,
                      elevation_distance=50, min_elevation=None, max_elevation=None):
        """
        Construction of the elevation bands based on the chosen method.

        Parameters
        ----------
        criteria : str|list
            The criteria to use to discretize the catchment (can be combined):
            'elevation' = elevation bands
            'aspect' = aspect according to the cardinal directions (4 classes)
        elevation_method : str
            The method to build the elevation bands:
            'isohypse' = fixed contour intervals (provide the 'distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'number' parameter)
        elevation_number : int, optional
            Number of elevation bands to create when using the 'quantiles' method.
        elevation_distance : int, optional
            Distance (m) between the contour lines when using the 'isohypse' method.
        min_elevation : int, optional
            Minimum elevation of the elevation bands (to homogenize between runs).
        max_elevation : int, optional
            Maximum elevation of the elevation bands (to homogenize between runs).
        """
        if not hb.has_pyproj:
            raise ImportError("pyproj is required to do this.")

        if isinstance(criteria, str):
            criteria = [criteria]

        if self.slope is None or self.aspect is None:
            self.calculate_slope_aspect()

        self.map_unit_ids = np.zeros(self.masked_dem_data.shape)

        # Create a dict to store the criteria
        criteria_dict = {}

        if 'elevation' in criteria:
            criteria_dict['elevation'] = []
            if elevation_method == 'isohypse':
                dist = elevation_distance
                if min_elevation is None:
                    min_elevation = np.nanmin(self.masked_dem_data)
                    min_elevation = min_elevation - (min_elevation % dist)
                if max_elevation is None:
                    max_elevation = np.nanmax(self.masked_dem_data)
                    max_elevation = max_elevation + (dist - max_elevation % dist)
                elevations = np.arange(min_elevation, max_elevation + dist, dist)
                for i in range(len(elevations) - 1):
                    criteria_dict['elevation'].append(elevations[i:i + 2])
            elif elevation_method == 'quantiles':
                elevations = np.nanquantile(
                    self.masked_dem_data, np.linspace(0, 1, num=elevation_number))
                for i in range(len(elevations) - 1):
                    criteria_dict['elevation'].append(elevations[i:i + 2])
            else:
                raise ValueError("Unknown elevation band creation method.")

        if 'aspect' in criteria:
            criteria_dict['aspect'] = ['N', 'E', 'S', 'W']

        res_elevation = []
        res_elevation_min = []
        res_elevation_max = []
        res_aspect_class = []

        combinations = list(itertools.product(*criteria_dict.values()))
        combinations_keys = criteria_dict.keys()

        unit_id = 1
        for i, criteria in enumerate(combinations):
            mask_unit = np.ones(self.masked_dem_data.shape, dtype=bool)
            # Mask nan values
            mask_unit[np.isnan(self.masked_dem_data)] = False

            for criterion_name, criterion in zip(combinations_keys, criteria):
                if criterion_name == 'elevation':
                    mask_elev = np.logical_and(
                        self.masked_dem_data >= criterion[0],
                        self.masked_dem_data < criterion[1])
                    mask_unit = np.logical_and(mask_unit, mask_elev)
                elif criterion_name == 'aspect':
                    if criterion == 'N':
                        mask_aspect = np.logical_or(
                            np.logical_and(self.aspect >= 315, self.aspect <= 360),
                            np.logical_and(self.aspect >= 0, self.aspect < 45))
                    elif criterion == 'E':
                        mask_aspect = np.logical_and(self.aspect >= 45,
                                                     self.aspect < 135)
                    elif criterion == 'S':
                        mask_aspect = np.logical_and(self.aspect >= 135,
                                                     self.aspect < 225)
                    elif criterion == 'W':
                        mask_aspect = np.logical_and(self.aspect >= 225,
                                                     self.aspect < 315)
                    else:
                        raise ValueError("Unknown aspect value.")
                    mask_unit = np.logical_and(mask_unit, mask_aspect)

            # If the unit is empty, skip it
            if np.count_nonzero(mask_unit) == 0:
                continue

            # Check that all cells in unit_ids are 0
            assert np.count_nonzero(self.map_unit_ids[mask_unit]) == 0

            # Set the unit id
            self.map_unit_ids[mask_unit] = unit_id

            # Set the mean elevation of the unit if elevation is a criterion
            if 'elevation' in criteria_dict.keys():
                i = list(combinations_keys).index('elevation')
                elevations = criteria[i]
                res_elevation.append(round(float(np.mean(elevations)), 2))
                res_elevation_min.append(round(float(elevations[0]), 2))
                res_elevation_max.append(round(float(elevations[1]), 2))

            # Get the aspect class if aspect is a criterion
            if 'aspect' in criteria_dict.keys():
                i = list(combinations_keys).index('aspect')
                res_aspect_class.append(criteria[i])

            unit_id += 1

        self.map_unit_ids = self.map_unit_ids.astype(hb.rasterio.uint16)

        if res_elevation:
            self.hydro_units.add_property(('elevation', 'm'), res_elevation)
            self.hydro_units.add_property(('elevation_min', 'm'), res_elevation_min)
            self.hydro_units.add_property(('elevation_max', 'm'), res_elevation_max)

        if res_aspect_class:
            self.hydro_units.add_property(('aspect_class', '-'), res_aspect_class)

        self.get_hydro_units_attributes()

    def get_hydro_units_attributes(self):
        """
        Extract the hydro units attributes.

        Returns
        -------
        The hydro units attributes.
        """
        if not hb.has_pyproj:
            raise ImportError("pyproj is required to do this.")

        if self.slope is None or self.aspect is None:
            self.calculate_slope_aspect()

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
            res_elevation_mean.append(self._extract_unit_mean_elevation(mask_unit))
            if not self.hydro_units.has('elevation'):
                res_elevation.append(self._extract_unit_mean_elevation(mask_unit))
                res_elevation_min.append(self._extract_unit_min_elevation(mask_unit))
                res_elevation_max.append(self._extract_unit_max_elevation(mask_unit))

            # Compute the slope and aspect of the unit
            res_slope.append(self._extract_unit_mean_slope(mask_unit))
            res_aspect.append(self._extract_unit_mean_aspect(mask_unit))

            # Calculate the mean unit coordinates in lat/lon
            lat, lon = self._extract_unit_mean_lat_lon(mask_unit)
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

    def get_mean_elevation(self):
        """
        Get the catchment mean elevation.

        Returns
        -------
        The catchment mean elevation.
        """
        return np.nanmean(self.masked_dem_data)

    def calculate_slope_aspect(self):
        """
        Calculate the slope and aspect of the whole DEM.
        """
        if not hb.has_xrspatial:
            raise ImportError("xarray-spatial is required to do this.")

        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            xr_dem = hb.rxr.open_rasterio(self.dem.files[0]).drop_vars('band')[0]
            self.slope = hb.xrs.slope(xr_dem, name='slope').to_numpy()
            self.aspect = hb.xrs.aspect(xr_dem, name='aspect').to_numpy()

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

    def save_unit_ids_raster(self, path):
        """
        Save the unit ids raster to a file.

        Parameters
        ----------
        path : str|Path
            Path to the output file.
        """
        if self.map_unit_ids is None:
            raise ValueError("No unit ids raster to save.")

        # Create the profile
        profile = self.dem.profile
        profile.update(
            dtype=hb.rasterio.uint16,
            count=1,
            compress='lzw',
            nodata=0)

        with hb.rasterio.open(path, 'w', **profile) as dst:
            dst.write(self.map_unit_ids, 1)

    def load_unit_ids_from_raster(self, path):
        """
        Load hydro units from a raster file.

        Parameters
        ----------
        path : str|Path
            Path to the raster file containing the hydro unit ids.
        """
        if not hb.has_rasterio:
            raise ImportError("rasterio is required to do this.")

        with hb.rasterio.open(path) as src:
            self._check_crs(src)
            geoms = [mapping(self.outline)]
            self.map_unit_ids, _ = mask(src, geoms, crop=False)
            self.map_unit_ids[self.map_unit_ids == src.nodata] = 0

            if len(self.map_unit_ids.shape) == 3:
                self.map_unit_ids = self.map_unit_ids[0]

            self.map_unit_ids = self.map_unit_ids.astype(hb.rasterio.uint16)

    def create_behaviour_land_cover_change_for_glaciers(self, whole_glaciers,
                                                        debris_glaciers, times,
                                                        with_debris=False):
        """
        Extract the glacier cover changes from shapefiles, creates a
        BehaviourLandCoverChange object, and assign the computed land cover
        changes to the BehaviourLandCoverChange object. Finally, initialize
        the HydroUnits cover with the first cover values of the
        BehaviourLandCoverChange object.

        Parameters
        ----------
        whole_glaciers : str|Path
            Path to the shapefile containing the extent of the glaciers
            (debris-covered and clean ice together).
        debris_glaciers : str|Path
            Path to the shapefile containing the extent of the debris-covered
            glaciers.
        times : str|list
            Date of the land cover, in the format: yyyy-mm-dd.
        with_debris : bool, optional
            True if the simulation requires debris-covered and clean-ice area
            computations, False otherwise.

        Returns
        -------
        changes : BehaviourLandCoverChange
            A BehaviourLandCoverChange object setup with the cover areas
            extracted from the shapefiles.
        """

        if not hb.has_geopandas:
            raise ImportError("geopandas is required to do this.")

        if self.map_unit_ids is None:
            raise ValueError("The catchment has not been discretized "
                             "(unit ids map missing).")

        # Get the unit ids
        unit_ids = np.unique(self.map_unit_ids)
        unit_ids = unit_ids[unit_ids != 0]



        unit_ids = unit_ids - 1

        glacier_df = pd.DataFrame(index=unit_ids)
        ice_df = pd.DataFrame(index=unit_ids)
        debris_df = pd.DataFrame(index=unit_ids)
        other_df = pd.DataFrame(index=unit_ids)

        glacier_df[0] = self.hydro_units.hydro_units.elevation
        ice_df[0] = self.hydro_units.hydro_units.elevation
        debris_df[0] = self.hydro_units.hydro_units.elevation
        other_df[0] = self.hydro_units.hydro_units.elevation

        for i, (whole_glacier, debris_glacier) in \
                enumerate(zip(whole_glaciers, debris_glaciers), 1):
            glacier, ice, debris, other = \
                self._extract_glacier_cover_change(whole_glacier, debris_glacier)

            glacier_df[i] = glacier
            ice_df[i] = ice
            debris_df[i] = debris
            other_df[i] = other

        glacier_df = self._format_dataframe(glacier_df, times, 'glacier')
        ice_df = self._format_dataframe(ice_df, times, 'glacier_ice')
        debris_df = self._format_dataframe(debris_df, times, 'glacier_debris')
        other_df = self._format_dataframe(other_df, times, 'ground')

        changes = hb.behaviours.BehaviourLandCoverChange()
        if with_debris:
            changes._match_hydro_unit_ids(debris_df, self.hydro_units,
                                          match_with='elevation')
            changes._remove_rows_with_no_changes(debris_df)
            changes._extract_changes(area_unit="m2", file_content=debris_df)

            changes._match_hydro_unit_ids(ice_df, self.hydro_units,
                                          match_with='elevation')
            changes._remove_rows_with_no_changes(ice_df)
            changes._extract_changes(area_unit="m2", file_content=ice_df)

        else:
            changes._match_hydro_unit_ids(glacier_df, self.hydro_units,
                                          match_with='elevation')
            changes._remove_rows_with_no_changes(glacier_df)
            changes._extract_changes(area_unit="m2", file_content=glacier_df)

        changes._match_hydro_unit_ids(other_df, self.hydro_units,
                                      match_with='elevation')
        changes._remove_rows_with_no_changes(other_df)
        changes._extract_changes(area_unit="m2", file_content=other_df)

        # Initialization of the cover before any change.
        hydro = self.hydro_units.hydro_units
        if with_debris:
            hydro[('area_debris', 'm2')] = debris_df[debris_df.columns[2]].values[2:]
            hydro[('area_ice', 'm2')] = ice_df[ice_df.columns[2]].values[2:]
        else:
            hydro[('area_glacier', 'm2')] = glacier_df[glacier_df.columns[2]].values[2:]
        hydro[('area_ground', 'm2')] = other_df[other_df.columns[2]].values[2:]

        return changes

    def _extract_glacier_cover_change(self, whole_glaciers_shapefile,
                                      debris_glaciers_shapefile,
                                      method='vectorial'):
        """
        Extract the glacier cover changes from shapefiles.

        Parameters
        ----------
        whole_glaciers_shapefile : str|Path
            Path to the shapefile containing the extent of the glaciers
            (debris-covered and clean ice together).
        debris_glaciers_shapefile : str|Path
            Path to the shapefile containing the extent of the debris-covered
            glaciers.
        method : str, optional
            The method to extract the glacier cover changes:
            'vectorial' = vectorial extraction (more precise)
            'raster' = raster extraction (faster)

        Returns
        -------
        glacier_area : array
            Area covered by glacier for each HydroUnit.
        bare_ice_area : array
            Area covered by clean-ice glacier for each HydroUnit.
        debris_area : array
            Area covered by debris-covered glacier for each HydroUnit.
        other_area : array
            Area covered by rock for each HydroUnit.
        """
        if method not in ['vectorial', 'raster']:
            raise ValueError("Unknown method.")

        # Clip the glaciers to the catchment extent
        all_glaciers = hb.gpd.read_file(whole_glaciers_shapefile)
        all_glaciers.to_crs(self.crs, inplace=True)
        glaciers = hb.gpd.clip(all_glaciers, self.outline)

        # Merge the glacier polygons
        glaciers['new_col'] = 0
        glaciers = glaciers.dissolve(by='new_col', as_index=False)

        # Drop all columns except the geometry
        glaciers = glaciers[['geometry']]

        # Compute the glaciated area of the catchment
        glaciated_area = self._compute_area(glaciers)
        non_glaciated_area = self.area - glaciated_area

        # Compute the debris-covered area of the glacier
        glaciers_debris = None
        if debris_glaciers_shapefile is not None:
            all_debris_glaciers = hb.gpd.read_file(debris_glaciers_shapefile)
            all_debris_glaciers.to_crs(self.crs, inplace=True)
            glaciers_debris = hb.gpd.clip(all_debris_glaciers, glaciers)

            # Merge the glacier debris polygons
            glaciers_debris['new_col'] = 0
            glaciers_debris = glaciers_debris.dissolve(by='new_col', as_index=False)

            # Drop all columns except the geometry
            glaciers_debris = glaciers_debris[['geometry']]

        # Display some glacier statistics
        m2 = Unit.M2
        km2 = Unit.KM2
        print(f"The catchment has an area of {convert_unit(self.area, m2, km2):.1f} km²"
              f", from which {convert_unit(glaciated_area, m2, km2):.1f} km² are "
              f"glaciated, and {convert_unit(non_glaciated_area, m2, km2):.1f} km² are "
              f"non glaciated.")

        print(f"The catchment is {glaciated_area / self.area * 100:.1f}% glaciated.")

        if debris_glaciers_shapefile is not None:
            debris_glaciated_area = self._compute_area(glaciers_debris)
            bare_ice_area = glaciated_area - debris_glaciated_area
            bare_ice_percentage = bare_ice_area / glaciated_area * 100
            print(f"The glaciers have {convert_unit(bare_ice_area, m2, km2):.1f} km² "
                  f"of bare ice, and {convert_unit(debris_glaciated_area, m2, km2):.1f}"
                  f" km² of debris-covered ice, thus amounting to "
                  f"{bare_ice_percentage:.1f}% of bare ice.")

        # Extract the pixel size
        x_size, y_size = abs(self.dem.transform[0]), abs(self.dem.transform[4])
        px_area = x_size * y_size

        # Define the method to extract the pixels touching the glaciers
        if method == 'vectorial':
            all_touched = True  # Needs to be True to include partly-covered pixels
        else:
            all_touched = False
            print(f"The dataset in the CRS {glaciers.crs} has a spatial resolution of "
                  f"{x_size} m, {y_size} m thus giving pixel areas "
                  f"of {px_area} m².")

        # Get the glacier mask
        glaciers_mask = self._mask_dem(glaciers, 0, all_touched=all_touched)
        debris_mask = None
        if debris_glaciers_shapefile is not None:
            debris_mask = self._mask_dem(glaciers_debris, 0, all_touched=all_touched)

        unit_ids = np.unique(self.map_unit_ids)
        unit_ids = unit_ids[unit_ids != 0]

        glacier_area = np.zeros(len(unit_ids))
        bare_ice_area = np.zeros(len(unit_ids))
        debris_area = np.zeros(len(unit_ids))
        other_area = np.zeros(len(unit_ids))

        for idx, unit_id in enumerate(unit_ids):
            mask_unit = self.map_unit_ids == unit_id
            unit_area = np.sum(mask_unit) * px_area

            if method == 'vectorial':
                warnings.filterwarnings(
                    "ignore", category=RuntimeWarning,
                    message="invalid value encountered in intersection")

                # Create an empty list to store the intersecting geometries
                intersecting_geom_glaciers = []
                intersecting_geom_debris = []

                # Iterate through the rows and columns of the raster
                for i in range(self.dem.height):
                    for j in range(self.dem.width):
                        # Check if there is a glacier pixel
                        if glaciers_mask[i, j] == 0:
                            continue

                        # Check if the pixel value matches the target value
                        if self.map_unit_ids[i, j] != unit_id:
                            continue

                        # Create a polygon for the pixel
                        xy = self.dem.xy(i, j)
                        x_min = xy[0] - x_size/2
                        y_min = xy[1] - y_size/2
                        x_max = xy[0] + x_size/2
                        y_max = xy[1] + y_size/2
                        pixel_geo = box(x_min, y_min, x_max, y_max)

                        # Iterate through glacier polygons and find intersections
                        for index, glacier_geom in glaciers.iterrows():
                            intersection = pixel_geo.intersection(
                                glacier_geom['geometry'])
                            if not intersection.is_empty:
                                intersecting_geom_glaciers.append(intersection)

                        # Iterate through debris polygons and find intersections
                        if debris_glaciers_shapefile is not None:
                            for index, debris_geom in glaciers_debris.iterrows():
                                intersection = pixel_geo.intersection(
                                    debris_geom['geometry'])
                                if not intersection.is_empty:
                                    intersecting_geom_debris.append(intersection)

                warnings.resetwarnings()

                if len(intersecting_geom_glaciers) > 0:
                    # Create a single geometry from all intersecting geometries
                    merged_geometry = unary_union(intersecting_geom_glaciers)
                    # Calculate the total area of the merged geometry
                    glacier_area[idx] = merged_geometry.area

                if len(intersecting_geom_debris) > 0:
                    # Create a single geometry from all intersecting geometries
                    merged_geometry = unary_union(intersecting_geom_debris)
                    # Calculate the total area of the merged geometry
                    debris_area[idx] = merged_geometry.area

                bare_ice_area[idx] = glacier_area[idx] - debris_area[idx]

            elif method == 'raster':
                glacier_area[idx] = np.count_nonzero(
                    glaciers_mask[mask_unit]) * px_area

                if debris_glaciers_shapefile is not None:
                    debris_area[idx] = np.count_nonzero(
                        debris_mask[mask_unit]) * px_area
                    bare_ice_area[idx] = glacier_area[idx] - debris_area[idx]

            other_area[idx] = unit_area - glacier_area[idx]

        print(f"After shapefile extraction (method: {method}), the glaciers have "
              f"{convert_unit(np.sum(bare_ice_area), m2, km2):.1f} km² of bare ice, "
              f"{convert_unit(np.sum(debris_area), m2, km2):.1f} km² of "
              f"debris-covered ice, and "
              f"{convert_unit(np.sum(other_area), m2, km2):.1f} km² of "
              f"non-glaciated area.")

        return glacier_area, bare_ice_area, debris_area, other_area

    def _mask_dem(self, shapefile, nodata, all_touched=False):
        geoms = []
        for geo in shapefile.geometry.values:
            geoms.append(mapping(geo))
        dem_clipped, _ = mask(self.dem, geoms, crop=False, all_touched=all_touched)
        dem_clipped[dem_clipped == self.dem.nodata] = nodata
        if len(dem_clipped.shape) == 3:
            dem_clipped = dem_clipped[0]

        return dem_clipped

    def _extract_area(self, outline):
        # The outline has to be given in meters.
        if not outline:
            return
        shapefile = hb.gpd.read_file(outline)
        self._check_crs(shapefile)
        area = self._compute_area(shapefile)
        self.area = area

    def _extract_outline(self, outline):
        if not outline:
            return
        shapefile = hb.gpd.read_file(outline)
        self._check_crs(shapefile)
        geoms = shapefile.geometry.values
        self.outline = geoms[0]

    def _extract_unit_mean_aspect(self, mask_unit):
        aspect_rad = np.radians(self.aspect[mask_unit])
        circular_mean_aspect_rad = math.atan2(np.mean(np.sin(aspect_rad)),
                                              np.mean(np.cos(aspect_rad)))
        circular_mean_aspect_deg = np.degrees(circular_mean_aspect_rad)
        if circular_mean_aspect_deg < 0:
            circular_mean_aspect_deg = circular_mean_aspect_deg + 360

        return circular_mean_aspect_deg

    def _extract_unit_mean_slope(self, mask_unit):
        return round(float(np.nanmean(self.slope[mask_unit])), 2)

    def _extract_unit_mean_elevation(self, mask_unit):
        return round(float(np.nanmean(self.masked_dem_data[mask_unit])), 2)

    def _extract_unit_min_elevation(self, mask_unit):
        return round(float(np.nanmin(self.masked_dem_data[mask_unit])), 2)

    def _extract_unit_max_elevation(self, mask_unit):
        return round(float(np.nanmax(self.masked_dem_data[mask_unit])), 2)

    def _extract_unit_mean_lat_lon(self, mask_unit):
        # Get rows and cols of the unit
        rows, cols = np.where(mask_unit)

        # Get coordinates of the unit
        xs, ys = hb.rasterio.transform.xy(self.dem.transform, list(rows), list(cols))

        # Get the mean coordinates of the unit
        mean_x = np.nanmean(xs)
        mean_y = np.nanmean(ys)

        # Get the mean coordinates of the unit in lat/lon
        transformer = hb.pyproj.Transformer.from_crs(self.crs, 4326)
        mean_lon, mean_lat = transformer.transform(mean_x, mean_y)

        return mean_lat, mean_lon

    def _check_crs(self, data):
        data_crs = self._get_crs_from_file(data)
        if self.crs is None:
            self.crs = data_crs
        else:
            if self.crs != data_crs:
                raise ValueError("The CRS of the data does not match the CRS of the "
                                 "catchment.")

    @staticmethod
    def _compute_area(shapefile):
        area = 0
        for _, row in shapefile.iterrows():
            poly_area = row.geometry.area
            area += poly_area
        return area

    @staticmethod
    def _format_dataframe(df, times, cover_name):
        df.loc[-0.5] = [np.nan] + times
        df = df.sort_index().reset_index(drop=True)
        df.loc[-0.5] = ['bands'] + [cover_name] * len(times)
        df = df.sort_index().reset_index(drop=True)
        df.insert(loc=0, column='hydro_unit', value=0)
        return df

    @staticmethod
    def _get_crs_from_file(data):
        if isinstance(data, hb.rasterio.DatasetReader):
            return data.crs
        elif isinstance(data, hb.gpd.GeoDataFrame):
            return data.crs
        else:
            raise ValueError("Unknown data format.")
