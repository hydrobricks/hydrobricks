import warnings

import numpy as np
import pandas as pd

import _hydrobricks as _hb
import hydrobricks as hb

from ..units import Unit, convert_unit
from .behaviour import Behaviour

if hb.has_shapely:
    from shapely.geometry import mapping
    from shapely.ops import unary_union

if hb.has_rasterio:
    from rasterio.mask import mask

m2 = Unit.M2
km2 = Unit.KM2


class BehaviourLandCoverChange(Behaviour):
    """Class for the land cover changes."""

    def __init__(self):
        super().__init__()
        self.behaviour = _hb.BehaviourLandCoverChange()

    def load_from_csv(self, path, hydro_units, area_unit, match_with='elevation'):
        """
        Read hydro units properties from csv file. The first column of the file must
        contain the information to identify the hydro unit id, such as the id or the
        elevation (when using elevation bands). The next columns must contain the
        changes at different dates for each hydro unit. The first line must contain
        the name of the land cover to change. The second line must contain the date
        of the change in a format easily parsed by Python.

        Parameters
        ----------
        path : str|Path
            Path to the csv file containing hydro units data.
        hydro_units : HydroUnits
            The hydro units to match the land cover changes against.
        area_unit: str
            Unit for the area values: "m2" or "km2"
        match_with : str
            Information used to identify the hydro units. Options: 'elevation', 'id'

        Example of a file (with areas in km2)
        -----------------
        elevation   glacier      glacier      glacier      glacier      glacier
                    2020-08-01   2025-08-01   2030-08-01   2035-08-01   2040-08-01
        4274        0.013        0.003        0            0            0
        4310        0.019        0.009        0            0            0
        4346        0.052        0.042        0.032        0.022        0.012
        4382        0.072        0.062        0.052        0.042        0.032
        4418        0.129        0.119        0.109        0.099        0.089
        4454        0.252        0.242        0.232        0.222        0.212
        4490        0.288        0.278        0.268        0.258        0.248
        4526        0.341        0.331        0.321        0.311        0.301
        4562        0.613        0.603        0.593        0.583        0.573
        """
        file_content = pd.read_csv(path, header=None)
        file_content.insert(loc=0, column='hydro_unit', value=0)

        self._match_hydro_unit_ids(file_content, hydro_units, match_with)
        self._remove_rows_with_no_changes(file_content)
        self._extract_changes(area_unit, file_content)

    def get_changes_nb(self):
        """
        Get the number of changes registered.
        """
        return self.behaviour.get_changes_nb()

    def get_land_covers_nb(self):
        """
        Get the number of land covers registered.
        """
        return self.behaviour.get_land_covers_nb()

    @staticmethod
    def create_behaviour_for_glaciers(catchment, full_glaciers, debris_glaciers,
                                      times, with_debris=False, method='vector'):
        """
        Extract the glacier cover changes from shapefiles, creates a
        BehaviourLandCoverChange object, and assign the computed land cover
        changes to the BehaviourLandCoverChange object. Finally, initialize
        the HydroUnits cover with the first cover values of the
        BehaviourLandCoverChange object.

        Parameters
        ----------
        catchment : Catchment
            The catchment to extract the glacier cover changes for.
        full_glaciers : str|Path
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
        method : str, optional
            The method to extract the glacier cover changes:
            'vector' = vectorial extraction (more precise)
            'raster' = raster extraction (faster)

        Returns
        -------
        changes : BehaviourLandCoverChange
            A BehaviourLandCoverChange object setup with the cover areas
            extracted from the shapefiles.
        changes_df : DataFrame
            A dataframe containing the cover areas extracted from the shapefiles.
        """

        if not hb.has_geopandas:
            raise ImportError("geopandas is required to do this.")

        if catchment.map_unit_ids is None:
            raise ValueError("The catchment has not been discretized "
                             "(unit ids map missing).")

        if catchment.hydro_units is None:
            raise ValueError("The catchment has not been discretized "
                             "(hydro units missing).")

        changes = hb.behaviours.BehaviourLandCoverChange()
        changes_df = changes._create_behaviour_for_glaciers(
            catchment, full_glaciers, debris_glaciers, times, with_debris, method)

        return changes, changes_df

    def _create_behaviour_for_glaciers(self, catchment, full_glaciers, debris_glaciers,
                                       times, with_debris, method):

        if len(full_glaciers) != len(times):
            raise ValueError("The number of shapefiles and dates must be equal.")

        if with_debris and len(debris_glaciers) != len(times):
            raise ValueError("The number of shapefiles and dates must be equal.")

        # Get the hydro units
        n_unit_ids = catchment.get_hydro_units_nb()
        hydro_units = catchment.hydro_units

        # Create the dataframe
        changes_df = pd.DataFrame(index=range(n_unit_ids + 2))
        changes_df.insert(loc=0, column='hydro_unit', value=0)
        ids = hydro_units.hydro_units.id.values.squeeze()
        changes_df.hydro_unit.iloc[2:n_unit_ids + 2] = ids

        # Parse the files
        for glacier_shp, debris_shp, time in zip(full_glaciers, debris_glaciers, times):
            glacier, ice, debris, other = self._extract_glacier_cover_change(
                catchment, glacier_shp, debris_shp, method=method)

            if with_debris:
                self._add_column_to_dataframe(changes_df, 'glacier_ice', time)
                changes_df.loc[2:, changes_df.columns[-1]] = ice
                self._add_column_to_dataframe(changes_df, 'glacier_debris', time)
                changes_df.loc[2:, changes_df.columns[-1]] = debris
            else:
                self._add_column_to_dataframe(changes_df, 'glacier', time)
                changes_df.loc[2:, changes_df.columns[-1]] = glacier

            self._add_column_to_dataframe(changes_df, 'ground', time)
            changes_df.loc[2:, changes_df.columns[-1]] = other

        # Populate the bounded instance
        self._extract_changes('m2', changes_df)

        return changes_df

    def _extract_glacier_cover_change(self, catchment, glaciers_shapefile,
                                      debris_shapefile, method='vector'):
        """
        Extract the glacier cover changes from shapefiles.

        Parameters
        ----------
        catchment : Catchment
            The catchment to extract the glacier cover changes for.
        glaciers_shapefile : str|Path
            Path to the shapefile containing the extent of the glaciers
            (debris-covered and clean ice together).
        debris_shapefile : str|Path
            Path to the shapefile containing the extent of the debris-covered
            glaciers.
        method : str, optional
            The method to extract the glacier cover changes:
            'vector' = vectorial extraction (more precise)
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
        if method not in ['vector', 'raster']:
            raise ValueError("Unknown method.")

        # Clip the glaciers to the catchment extent
        all_glaciers = hb.gpd.read_file(glaciers_shapefile)
        all_glaciers.to_crs(catchment.crs, inplace=True)
        glaciers = hb.gpd.clip(all_glaciers, catchment.outline)
        glaciers = self._simplify_df_geometries(glaciers)

        # Compute the glaciated area of the catchment
        glaciated_area = hb.utils.compute_area(glaciers)
        non_glaciated_area = catchment.area - glaciated_area

        # Compute the debris-covered area of the glacier
        glaciers_debris = None
        if debris_shapefile is not None:
            all_debris_glaciers = hb.gpd.read_file(debris_shapefile)
            all_debris_glaciers.to_crs(catchment.crs, inplace=True)
            glaciers_debris = hb.gpd.clip(all_debris_glaciers, glaciers)
            glaciers_debris = self._simplify_df_geometries(glaciers_debris)

        # Display some glacier statistics
        print(f"The catchment has an area of "
              f"{convert_unit(catchment.area, m2, km2):.1f} km², from which "
              f"{convert_unit(glaciated_area, m2, km2):.1f} km² are glaciated, "
              f"and {convert_unit(non_glaciated_area, m2, km2):.1f} km² are "
              f"non glaciated.")

        print(f"The catchment is {glaciated_area / catchment.area * 100:.1f}% "
              f"glaciated.")

        if debris_shapefile is not None:
            debris_glaciated_area = hb.utils.compute_area(glaciers_debris)
            bare_ice_area = glaciated_area - debris_glaciated_area
            bare_ice_percentage = bare_ice_area / glaciated_area * 100
            print(f"The glaciers have {convert_unit(bare_ice_area, m2, km2):.1f} km² "
                  f"of bare ice, and {convert_unit(debris_glaciated_area, m2, km2):.1f}"
                  f" km² of debris-covered ice, thus amounting to "
                  f"{bare_ice_percentage:.1f}% of bare ice.")

        # Extract the pixel size
        x_size = catchment.get_dem_x_resolution()
        y_size = catchment.get_dem_y_resolution()
        px_area = catchment.get_dem_pixel_area()

        # Define the method to extract the pixels touching the glaciers
        if method == 'vector':
            all_touched = True  # Needs to be True to include partly-covered pixels
        else:
            all_touched = False
            print(f"The dataset in the CRS {glaciers.crs} has a spatial resolution of "
                  f"{x_size} m, {y_size} m thus giving pixel areas "
                  f"of {px_area} m².")

        # Get the glacier mask
        glaciers_mask = self._mask_dem(catchment, glaciers, 0,
                                       all_touched=all_touched)
        debris_mask = None
        if debris_shapefile is not None:
            debris_mask = self._mask_dem(catchment, glaciers_debris, 0,
                                         all_touched=all_touched)

        unit_ids = np.unique(catchment.map_unit_ids)
        unit_ids = unit_ids[unit_ids != 0]

        glacier_area = np.zeros(len(unit_ids))
        bare_ice_area = np.zeros(len(unit_ids))
        debris_area = np.zeros(len(unit_ids))
        other_area = np.zeros(len(unit_ids))

        for idx, unit_id in enumerate(unit_ids):
            mask_unit = catchment.map_unit_ids == unit_id
            unit_area = np.sum(mask_unit) * px_area

            if method == 'vector':
                warnings.filterwarnings(
                    "ignore", category=RuntimeWarning,
                    message="invalid value encountered in intersection")

                # Create an empty list to store the pixel geometries
                pixels_geoms = []

                # Iterate through the rows and columns of the raster
                for i in range(catchment.dem.height):
                    for j in range(catchment.dem.width):
                        # Check if there is a glacier pixel
                        if glaciers_mask[i, j] == 0:
                            continue

                        # Check if the pixel value matches the target value
                        if catchment.map_unit_ids[i, j] != unit_id:
                            continue

                        # Create a polygon for the pixel
                        pixel_geo = catchment.create_dem_pixel_geometry(i, j)
                        pixels_geoms.append(pixel_geo)

                unit_geoms = unary_union(pixels_geoms)

                # Iterate through glacier polygons and find intersections
                inters_glaciers = []
                if len(pixels_geoms) > 0:
                    self._get_intersections(unit_geoms, glaciers, inters_glaciers)

                # Iterate through debris polygons and find intersections
                inters_debris = []
                if len(pixels_geoms) > 0 and debris_shapefile is not None:
                    self._get_intersections(unit_geoms, glaciers_debris, inters_debris)

                warnings.resetwarnings()

                glacier_area[idx] = self._compute_intersection_area(inters_glaciers)
                debris_area[idx] = self._compute_intersection_area(inters_debris)
                bare_ice_area[idx] = glacier_area[idx] - debris_area[idx]

            elif method == 'raster':
                glacier_area[idx] = np.count_nonzero(
                    glaciers_mask[mask_unit]) * px_area

                if debris_shapefile is not None:
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

    @staticmethod
    def _add_column_to_dataframe(changes_df, land_cover, time):
        # Add column with an incremental index
        changes_df[len(changes_df.columns)] = 0
        # Add the land cover name to the first row
        changes_df.loc[0, changes_df.columns[-1]] = land_cover
        # Add the date to the second row
        changes_df.loc[1, changes_df.columns[-1]] = time

    @staticmethod
    def _compute_intersection_area(intersecting_geoms):
        if len(intersecting_geoms) == 0:
            return 0

        # Create a single geometry from all intersecting geometries
        merged_geometry = unary_union(intersecting_geoms)

        return merged_geometry.area

    @staticmethod
    def _get_intersections(pixel_geo, objects, intersecting_geoms):
        for _, obj in objects.iterrows():
            intersection = pixel_geo.intersection(obj['geometry'])
            if not intersection.is_empty:
                intersecting_geoms.append(intersection)

    @staticmethod
    def _simplify_df_geometries(df):
        # Merge the polygons
        df['new_col'] = 0
        df = df.dissolve(by='new_col', as_index=False)
        # Drop all columns except the geometry
        df = df[['geometry']]

        return df

    @staticmethod
    def _mask_dem(catchment, shapefile, nodata, all_touched=False):
        geoms = []
        for geo in shapefile.geometry.values:
            geoms.append(mapping(geo))
        dem_masked, _ = mask(catchment.dem, geoms, crop=False, all_touched=all_touched)
        dem_masked[dem_masked == catchment.dem.nodata] = nodata
        if len(dem_masked.shape) == 3:
            dem_masked = dem_masked[0]

        return dem_masked

    @staticmethod
    def _match_hydro_unit_ids(file_content, hydro_units, match_with):
        hu_df = hydro_units.hydro_units
        for row, change in file_content.iterrows():
            if row < 2:
                continue
            if match_with == 'elevation':
                elevation_values = hu_df[('elevation', 'm')].values
                idx_id = hu_df.index[elevation_values == int(change[0])].to_list()[0]
            elif match_with == 'id':
                idx_id = int(change[0])
            else:
                raise ValueError(f'No option "{match_with}" for "match_with".')
            file_content.loc[row, 'hydro_unit'] = hu_df.loc[idx_id, ('id', '-')]

    @staticmethod
    def _remove_rows_with_no_changes(file_content):
        for row, change in file_content.iterrows():
            if row < 2:
                continue
            diff = change.to_numpy(dtype=float)[2:]
            diff = diff[0:-1] - diff[1:]
            for i_diff, v_diff in enumerate(diff):
                if v_diff == 0:
                    file_content.iloc[row, i_diff + 3] = np.nan

    def _extract_changes(self, area_unit, file_content):
        for col in list(file_content):
            if col == 'hydro_unit' or col == 0:
                continue
            land_cover = file_content.loc[0, col]
            date = pd.Timestamp(file_content.loc[1, col])
            mjd = hb.utils.date_as_mjd(date)

            for row in range(2, len(file_content[col])):
                hu_id = file_content.loc[row, 'hydro_unit']
                area = float(file_content.loc[row, col])
                if not np.isnan(area):
                    area = convert_unit(area, area_unit, Unit.M2)
                    self.behaviour.add_change(mjd, hu_id, land_cover, area)
