import itertools
import math
import warnings

import numpy as np

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
            geoms = [hb.mapping(self.outline)]
            src = hb.rasterio.open(dem_path)
            self._check_crs(src)
            self.dem = src
            self.masked_dem_data, _ = hb.mask(src, geoms, crop=False)
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
            geoms = [hb.mapping(self.outline)]
            self.map_unit_ids, _ = hb.mask(src, geoms, crop=False)
            self.map_unit_ids[self.map_unit_ids == src.nodata] = 0

            if len(self.map_unit_ids.shape) == 3:
                self.map_unit_ids = self.map_unit_ids[0]

            self.map_unit_ids = self.map_unit_ids.astype(hb.rasterio.uint16)

    def create_behaviour_land_cover_change(self, whole_glaciers, debris_glaciers,
                                            times, with_debris=False):
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
            Date of the land cover, in the format: dd/mm/yyyy.
        with_debris : boolean, optional
            True if the simulation requires debris-covered and clean-ice area
            computations, False otherwise.

        Returns
        -------
        changes : BehaviourLandCoverChange
            A BehaviourLandCoverChange object setup with the cover areas
            extracted from the shapefiles.
        """
        if not hb.has_pandas:
            raise ImportError("pandas is required to do this.")

        unit_ids = np.unique(self.map_unit_ids)
        unit_ids = unit_ids[unit_ids != 0] - 1

        debris_df = hb.pd.DataFrame(index = unit_ids)
        ice_df = hb.pd.DataFrame(index = unit_ids)
        rock_df = hb.pd.DataFrame(index = unit_ids)
        glacier_df = hb.pd.DataFrame(index = unit_ids)

        debris_df[0] = self.hydro_units.hydro_units.elevation
        ice_df[0] = self.hydro_units.hydro_units.elevation
        glacier_df[0] = self.hydro_units.hydro_units.elevation
        rock_df[0] = self.hydro_units.hydro_units.elevation

        for i, (whole_glacier, debris_glacier) in \
            enumerate(zip(whole_glaciers, debris_glaciers), 1):

            debris, ice, rock, glacier = \
                self._extract_glacier_cover_change(whole_glacier, debris_glacier)

            debris_df[i] = debris
            ice_df[i] = ice
            glacier_df[i] = glacier
            rock_df[i] = rock

        debris_df = self._format_dataframe(debris_df, times, 'glacier_debris')
        ice_df = self._format_dataframe(ice_df, times, 'glacier_ice')
        glacier_df = self._format_dataframe(glacier_df, times, 'glacier')
        rock_df = self._format_dataframe(rock_df, times, 'ground')

        changes = hb.behaviours.BehaviourLandCoverChange()
        if with_debris == True:
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

        changes._match_hydro_unit_ids(rock_df, self.hydro_units,
                                      match_with='elevation')
        changes._remove_rows_with_no_changes(rock_df)
        changes._extract_changes(area_unit="m2", file_content=rock_df)

        # Initialization of the cover before any change.
        hydro = self.hydro_units.hydro_units
        if with_debris == True:
            hydro[('area_debris', 'm2')] = debris_df[debris_df.columns[2]].values[2:]
            hydro[('area_ice', 'm2')] = ice_df[ice_df.columns[2]].values[2:]
        else:
            hydro[('area_glacier', 'm2')] = glacier_df[glacier_df.columns[2]].values[2:]
        hydro[('area_ground', 'm2')] = rock_df[rock_df.columns[2]].values[2:]

        return changes

    def _extract_glacier_cover_change(self, whole_glaciers_shapefile,
                                      debris_glaciers_shapefile):
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

        Returns
        -------
        debris_area : array
            Area covered by debris-covered glacier for each HydroUnit.
        bare_ice_area : array
            Area covered by clean-ice glacier for each HydroUnit.
        bare_rock_area : array
            Area covered by rock for each HydroUnit.
        glacier_area : array
            Area covered by glacier for each HydroUnit.
        """

        all_glaciers = hb.gpd.read_file(whole_glaciers_shapefile)
        all_glaciers.to_crs(self.crs, inplace=True)
        glaciers = hb.gpd.clip(all_glaciers, self.outline)

        glaciated_area = self._compute_area(glaciers)
        bare_rock_area = self.area - glaciated_area
        glaciated_percentage = glaciated_area / self.area * 100

        if debris_glaciers_shapefile != None:
            all_debris_glaciers = hb.gpd.read_file(debris_glaciers_shapefile)
            all_debris_glaciers.to_crs(self.crs, inplace=True)
            debris_glaciers = hb.gpd.clip(all_debris_glaciers, self.outline)
            debris_glaciers_only = hb.gpd.clip(debris_glaciers, glaciers)

            debris_glaciated_area = self._compute_area(debris_glaciers_only)
            bare_ice_area = glaciated_area - debris_glaciated_area
            bare_ice_percentage = bare_ice_area / glaciated_area * 100

        else:
            debris_glaciated_area = np.nan
            bare_ice_area = np.nan
            bare_ice_percentage = np.nan

        m2_to_km2 = 1/1000000
        print("The catchment has an area of {:.1f} km², from which "
              "{:.1f} km² are glaciated, and {:.1f} km² are bare rock."
              "".format(self.area * m2_to_km2, glaciated_area * m2_to_km2, \
                        bare_rock_area * m2_to_km2))
        print(f"The catchment is thus {glaciated_percentage:.1f}% glaciated.")
        if debris_glaciers_shapefile != None:
            print("The glaciers have {:.1f} km² of bare ice, and {:.1f} km² "
                  "of debris-covered ice, thus amounting to {:.1f}% of bare ice."
                  "".format(bare_ice_area * m2_to_km2, debris_glaciated_area * \
                            m2_to_km2, bare_ice_percentage))

        # Find pixel size
        a = self.dem.transform
        x, y = abs(a[0]), abs(a[4])
        pixel_area = x * y
        print('The dataset in', glaciers.crs, 'has a spatial resolution of',
              x, 'x', y, 'm thus giving pixel areas of', pixel_area, 'm².')

        glaciers_clipped = self._mask_dem(glaciers, 0)
        if debris_glaciers_shapefile != None:
            debris_clipped = self._mask_dem(debris_glaciers, 0)

        unit_ids = np.unique(self.map_unit_ids)
        unit_ids = unit_ids[unit_ids != 0]
        unit_nb = len(unit_ids)

        debris_area = np.zeros(unit_nb)
        bare_ice_area = np.zeros(unit_nb)
        bare_rock_area = np.zeros(unit_nb)
        glacier_area = np.zeros(unit_nb)

        for _, unit_id in enumerate(unit_ids):
            mask_unit = self.map_unit_ids == unit_id
            unit_area = np.sum(mask_unit) * pixel_area

            id = unit_id - 1

            topo_mask_glacier = glaciers_clipped[mask_unit]
            glacier_area[id] = np.count_nonzero(topo_mask_glacier) * pixel_area
            bare_rock_area[id] = unit_area - glacier_area[id]

            if debris_glaciers_shapefile != None:
                topo_mask_debris = debris_clipped[mask_unit]
                debris_area[id] = np.count_nonzero(topo_mask_debris) * pixel_area
                bare_ice_area[id] = glacier_area[id] - debris_area[id]

                print("After shapefile to raster conversion, the glaciers have "
                      "{:.1f} km² of bare ice, {:.1f} km² of debris-covered ice,"
                      " and {:.1f} km² of bare rock."
                      "".format(np.sum(bare_ice_area) * m2_to_km2, \
                                np.sum(debris_area) * m2_to_km2, \
                                np.sum(bare_rock_area) * m2_to_km2))
            else:
                debris_area[id] = np.nan
                bare_ice_area[id] = np.nan

        return debris_area, bare_ice_area, bare_rock_area, glacier_area

    def _mask_dem(self, shapefile, nodata):
        geoms = []
        for geo in shapefile.geometry.values:
            geoms.append(hb.mapping(geo))
        dem_clipped, _ = hb.mask(self.dem, geoms, crop=False)
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
