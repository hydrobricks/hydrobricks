from __future__ import annotations

import warnings
from pathlib import Path

import numpy as np
import pandas as pd

import _hydrobricks as _hb
import hydrobricks as hb
import hydrobricks.utils as utils
from hydrobricks.actions.action import Action
from hydrobricks.catchment import Catchment
from hydrobricks.hydro_units import HydroUnits
from hydrobricks.units import Unit, convert_unit

if hb.has_shapely:
    from shapely.geometry import MultiPolygon, mapping
    from shapely.ops import unary_union

if hb.has_rasterio:
    from rasterio.mask import mask

m2 = Unit.M2
km2 = Unit.KM2


class ActionLandCoverChange(Action):
    """Class for the land cover changes."""

    def __init__(self):
        super().__init__()
        self.action = _hb.ActionLandCoverChange()

    def load_from_csv(
            self,
            path: str | Path,
            hydro_units: HydroUnits,
            land_cover: str,
            area_unit: str,
            match_with: str = 'elevation'
    ):
        """
        Read land cover changes from a csv file. Such file should contain the
        changes for a single land cover. Multiple files can be loaded consecutively.
        The first column of the file must contain the information to identify the hydro
        unit id, such as the id or the elevation (when using elevation bands).
        The next columns must contain the changes at different dates for each hydro
        unit. The first line must contain the dates of the changes in a format easily
        parsed by Python (i.e. YYYY-MM-DD).

        Parameters
        ----------
        path
            Path to the csv file containing hydro units data.
        hydro_units
            The hydro units to match the land cover changes against.
        land_cover
            Name of the land cover to change.
        area_unit
            Unit for the area values: "m2" or "km2"
        match_with
            Information used to identify the hydro units. Options: 'elevation', 'id'

        Example of a file (with areas in km2)
        -----------------
        elevation   2020-08-01   2025-08-01   2030-08-01   2035-08-01   2040-08-01
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
        file_content = pd.read_csv(path)
        if match_with == 'id':
            # Check that the first column contains hydro unit ids
            col_1 = file_content.iloc[:, 0]
            id_min = hydro_units.hydro_units[('id', '-')].min()
            id_max = hydro_units.hydro_units[('id', '-')].max()
            if col_1.min() < id_min or col_1.max() > id_max:
                raise ValueError("The first column of the file does not contain the "
                                 "hydro unit ids.")
            # Set the first column name to 'hydro_unit'
            file_content.rename(
                columns={file_content.columns[0]: 'hydro_unit'},
                inplace=True
            )
        else:
            file_content.insert(loc=0, column='hydro_unit', value=0)
            self._match_hydro_unit_ids(file_content, hydro_units, match_with)
            # Drop 2nd column (e.g., elevation)
            file_content.drop(file_content.columns[1], axis=1, inplace=True)

        self._remove_rows_with_no_changes(file_content)
        self._populate_bounded_instance(land_cover, area_unit, file_content)

    def get_changes_nb(self) -> int:
        """
        Get the number of changes registered.
        """
        return self.action.get_changes_nb()

    def get_land_covers_nb(self) -> int:
        """
        Get the number of land covers registered.
        """
        return self.action.get_land_covers_nb()

    @classmethod
    def create_action_for_glaciers(
            cls,
            catchment: Catchment,
            times: list[str],
            full_glaciers: list[str] | Path,
            debris_glaciers: list[str] | Path | None = None,
            with_debris: bool = False,
            method: str = 'vector',
            interpolate_yearly: bool = True
    ) -> tuple[ActionLandCoverChange, list[pd.DataFrame]]:
        """
        Extract the glacier cover changes from shapefiles, creates a
        ActionLandCoverChange object, and assign the computed land cover
        changes to the ActionLandCoverChange object.

        Parameters
        ----------
        catchment
            The catchment to extract the glacier cover changes for.
        times
            Date of the land cover, in the format: yyyy-mm-dd.
        full_glaciers
            Path to the shapefile containing the extent of the glaciers
            (debris-covered and clean ice together).
        debris_glaciers
            Path to the shapefile containing the extent of the debris-covered
            glaciers.
        with_debris
            True if the simulation requires debris-covered and clean-ice area
            computations, False otherwise.
        method
            The method to extract the glacier cover changes:
            'vector' = vectorial extraction (more precise)
            'raster' = raster extraction (faster)
        interpolate_yearly
            True if the changes should be interpolated to a yearly time step,
            False otherwise (no interpolation).

        Returns
        -------
        changes
            A ActionLandCoverChange object setup with the cover areas
            extracted from the shapefiles.
        changes_df
            A list of the dataframes containing the cover areas extracted from the
            shapefiles. If with_debris is True, the list contains 3 dataframes:
            [glacier_ice, glacier_debris, ground]. If with_debris is False, the list
            contains 2 dataframes: [glacier, ground].
        """

        if not hb.has_geopandas:
            raise ImportError("geopandas is required to do this.")

        if catchment.map_unit_ids is None:
            raise ValueError("The catchment has not been discretized "
                             "(unit ids map missing).")

        if catchment.hydro_units is None:
            raise ValueError("The catchment has not been discretized "
                             "(hydro units missing).")

        changes = ActionLandCoverChange()
        changes_df = changes._create_action_for_glaciers(
            catchment,
            full_glaciers,
            debris_glaciers,
            times,
            with_debris,
            method,
            interpolate_yearly
        )

        return changes, changes_df

    def _create_action_for_glaciers(
            self,
            catchment: Catchment,
            full_glaciers: list[str] | Path,
            debris_glaciers: list[str] | Path | None,
            times: list[str],
            with_debris: bool,
            method: str,
            interpolate_yearly: bool
    ):

        if len(full_glaciers) != len(times):
            raise ValueError("The number of shapefiles and dates must be equal.")

        if with_debris and debris_glaciers is None:
            raise ValueError("The debris shapefiles must be provided.")

        if with_debris and len(debris_glaciers) != len(times):
            raise ValueError("The number of shapefiles and dates must be equal.")

        if not with_debris and debris_glaciers is None:
            debris_glaciers = [None] * len(times)

        # Get the hydro units
        n_unit_ids = catchment.get_hydro_units_nb()
        hydro_units = catchment.hydro_units

        # Create the dataframes
        glacier_df = self._create_new_change_dataframe(hydro_units, n_unit_ids)
        ice_df = self._create_new_change_dataframe(hydro_units, n_unit_ids)
        debris_df = self._create_new_change_dataframe(hydro_units, n_unit_ids)
        ground_df = self._create_new_change_dataframe(hydro_units, n_unit_ids)

        # Parse the files
        glacier_np = np.zeros((n_unit_ids, len(times)))
        ice_np = np.zeros((n_unit_ids, len(times)))
        debris_np = np.zeros((n_unit_ids, len(times)))
        ground_np = np.zeros((n_unit_ids, len(times)))
        for glacier_shp, debris_shp, time in zip(full_glaciers, debris_glaciers, times):
            print(f"Extracting glacier cover changes for {time}...")
            glacier, ice, debris, other = self._extract_glacier_cover_change(
                catchment,
                glacier_shp,
                debris_shp,
                method=method
            )
            glacier_np[:, times.index(time)] = glacier
            ice_np[:, times.index(time)] = ice
            debris_np[:, times.index(time)] = debris
            ground_np[:, times.index(time)] = other

        # Interpolate the data to yearly time steps
        times_full = pd.to_datetime(times)
        if interpolate_yearly:
            if with_debris:
                ice_np, _ = self._interpolate_yearly(ice_np, times)
                debris_np, _ = self._interpolate_yearly(debris_np, times)
            else:
                glacier_np, _ = self._interpolate_yearly(glacier_np, times)
            ground_np, times_full = self._interpolate_yearly(ground_np, times)

        # Add the columns to the dataframes
        times_full_str = [time.strftime('%Y-%m-%d') for time in times_full]
        empty_df = pd.DataFrame(columns=times_full_str)
        if with_debris:
            ice_df = pd.concat([ice_df, empty_df.copy()], axis=1)
            debris_df = pd.concat([debris_df, empty_df.copy()], axis=1)
        else:
            glacier_df = pd.concat([glacier_df, empty_df.copy()], axis=1)
        ground_df = pd.concat([ground_df, empty_df.copy()], axis=1)

        # Set data to the dataframes
        if with_debris:
            ice_df.iloc[:, 1:] = ice_np
            debris_df.iloc[:, 1:] = debris_np
        else:
            glacier_df.iloc[:, 1:] = glacier_np
        ground_df.iloc[:, 1:] = ground_np

        # Populate the bounded instance (not needed for the ground type)
        if with_debris:
            self._remove_rows_with_no_changes(ice_df)
            self._populate_bounded_instance('glacier_ice', 'm2', ice_df)
            self._remove_rows_with_no_changes(debris_df)
            self._populate_bounded_instance('glacier_debris', 'm2', debris_df)
        else:
            self._remove_rows_with_no_changes(glacier_df)
            self._populate_bounded_instance('glacier', 'm2', glacier_df)

        if with_debris:
            return [ice_df, debris_df, ground_df]
        else:
            return [glacier_df, ground_df]

    def _extract_glacier_cover_change(
            self,
            catchment: Catchment,
            glaciers_shapefile: str | Path,
            debris_shapefile: str | Path | None,
            method: str = 'vector'
    ) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
        """
        Extract the glacier cover changes from shapefiles.

        Parameters
        ----------
        catchment
            The catchment to extract the glacier cover changes for.
        glaciers_shapefile
            Path to the shapefile containing the extent of the glaciers
            (debris-covered and clean ice together).
        debris_shapefile
            Path to the shapefile containing the extent of the debris-covered
            glaciers.
        method
            The method to extract the glacier cover changes:
            'vector' = vectorial extraction (more precise)
            'raster' = raster extraction (faster)

        Returns
        -------
        glacier_area
            Area covered by glacier for each HydroUnit.
        bare_ice_area
            Area covered by clean-ice glacier for each HydroUnit.
        debris_area
            Area covered by debris-covered glacier for each HydroUnit.
        other_area
            Area covered by rock for each HydroUnit.
        """
        if method not in ['vector', 'raster']:
            raise ValueError("Unknown method.")

        # Clip the glaciers to the catchment extent
        all_glaciers = hb.gpd.read_file(glaciers_shapefile)
        all_glaciers.to_crs(catchment.crs, inplace=True)
        if catchment.outline[0].geom_type == 'MultiPolygon':
            glaciers = hb.gpd.clip(all_glaciers, catchment.outline[0])
        elif catchment.outline[0].geom_type == 'Polygon':
            glaciers = hb.gpd.clip(all_glaciers, MultiPolygon(catchment.outline))
        else:
            raise ValueError("The catchment outline must be a (multi)polygon.")
        glaciers = self._simplify_df_geometries(glaciers)

        # Compute the glaciated area of the catchment
        glaciated_area = utils.compute_area(glaciers)
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
            debris_glaciated_area = utils.compute_area(glaciers_debris)
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
        glaciers_mask = self._mask_dem(
            catchment,
            glaciers,
            nodata=0,
            all_touched=all_touched
        )
        debris_mask = None
        if debris_shapefile is not None:
            debris_mask = self._mask_dem(
                catchment,
                glaciers_debris,
                nodata=0,
                all_touched=all_touched
            )

        unit_ids = catchment.hydro_units.hydro_units[('id', '-')].values

        glacier_area = np.zeros(len(unit_ids))
        bare_ice_area = np.zeros(len(unit_ids))
        debris_area = np.zeros(len(unit_ids))
        other_area = np.zeros(len(unit_ids))

        for idx, unit_id in enumerate(unit_ids):
            mask_unit = catchment.map_unit_ids == unit_id
            unit_area = np.sum(mask_unit) * px_area

            if method == 'vector':
                warnings.filterwarnings(
                    "ignore",
                    category=RuntimeWarning,
                    message="invalid value encountered in intersection"
                )

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
    def _interpolate_yearly(
            data: np.ndarray,
            times: list[str]
    ) -> tuple[np.ndarray, pd.DatetimeIndex]:
        # Transform the times to datetime instances
        times_dt = pd.to_datetime(times)
        times_full = pd.date_range(times_dt[0], times_dt[-1], freq='YS')

        # Create an array with the interpolated values
        data_full = np.zeros((data.shape[0], len(times_full)))
        for idx, _ in enumerate(data):
            data_full[idx, :] = np.interp(times_full, times_dt, data[idx, :])

        return data_full, times_full

    @staticmethod
    def _create_new_change_dataframe(
            hydro_units: HydroUnits,
            n_unit_ids: int
    ) -> pd.DataFrame:
        changes_df = pd.DataFrame(index=range(n_unit_ids))
        changes_df.insert(loc=0, column='hydro_unit', value=0)
        ids = hydro_units.hydro_units[('id', '-')].values.squeeze()
        changes_df.loc[:, 'hydro_unit'] = ids
        return changes_df

    @staticmethod
    def _compute_intersection_area(intersecting_geoms: list[MultiPolygon]) -> float:
        if len(intersecting_geoms) == 0:
            return 0

        # Create a single geometry from all intersecting geometries
        merged_geometry = unary_union(intersecting_geoms)

        return merged_geometry.area

    @staticmethod
    def _get_intersections(
            pixel_geo: MultiPolygon,
            objects: hb.gpd.GeoDataFrame,
            intersecting_geoms: list[MultiPolygon]
    ):
        for _, obj in objects.iterrows():
            intersection = pixel_geo.intersection(obj['geometry'])
            if not intersection.is_empty:
                intersecting_geoms.append(intersection)

    @staticmethod
    def _simplify_df_geometries(df: hb.gpd.GeoDataFrame) -> hb.gpd.GeoDataFrame:
        # Merge the polygons
        df['new_col'] = 0
        df = df.dissolve(by='new_col', as_index=False)
        # Drop all columns except the geometry
        df = df[['geometry']]

        return df

    @staticmethod
    def _mask_dem(
            catchment: Catchment,
            shapefile: hb.gpd.GeoDataFrame,
            nodata: float,
            all_touched: bool = False
    ) -> np.ndarray:
        geoms = []
        for geo in shapefile.geometry.values:
            geoms.append(mapping(geo))
        dem_masked, _ = mask(catchment.dem, geoms, crop=False, all_touched=all_touched)
        dem_masked[dem_masked == catchment.dem.nodata] = nodata
        if len(dem_masked.shape) == 3:
            dem_masked = dem_masked[0]

        return dem_masked

    @staticmethod
    def _match_hydro_unit_ids(
            file_content: pd.DataFrame,
            hydro_units: HydroUnits,
            match_with: str
    ):
        hu_df = hydro_units.hydro_units
        for row, change in file_content.iterrows():
            if match_with == 'elevation':
                elevation_values = hu_df[('elevation', 'm')].to_numpy(dtype=np.int64)
                value = int(change.iloc[1])
                idx_id = hu_df.index[elevation_values == value].to_list()[0]
            else:
                raise ValueError(f'No option "{match_with}" for "match_with".')
            file_content.at[row, 'hydro_unit'] = hu_df.at[idx_id, ('id', '-')]

    @staticmethod
    def _remove_rows_with_no_changes(file_content: pd.DataFrame):
        for idx, (row, change) in enumerate(file_content.iterrows()):
            diff = change.to_numpy(dtype=float)[1:]
            diff = diff[0:-1] - diff[1:]
            for i_diff, v_diff in enumerate(diff):
                if v_diff == 0:
                    file_content.iloc[idx, i_diff + 2] = np.nan

    def _populate_bounded_instance(
            self,
            land_cover: str,
            area_unit: str,
            file_content: pd.DataFrame
    ):
        for col in list(file_content):
            if col == 'hydro_unit' or col == 0:
                continue
            # Extract date from column name
            mjd = utils.date_as_mjd(col)

            for row in range(len(file_content[col])):
                hu_id = file_content.loc[row, 'hydro_unit']
                area = float(file_content.loc[row, col])
                if not np.isnan(area):
                    area = convert_unit(area, area_unit, Unit.M2)
                    self.action.add_change(mjd, hu_id, land_cover, area)
