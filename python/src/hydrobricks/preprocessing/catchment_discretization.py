from __future__ import annotations

import itertools
from pathlib import Path
from typing import TYPE_CHECKING

import numpy as np

from hydrobricks._exceptions import (
    ConfigurationError,
    DataError,
    DependencyError,
    ModelError,
)
from hydrobricks._optional import HAS_GEOPANDAS, HAS_PYPROJ, HAS_SCIPY, gpd, rasterio

if HAS_SCIPY:
    from scipy import ndimage

if TYPE_CHECKING:
    from hydrobricks.catchment import Catchment


class CatchmentDiscretization:
    """
    Class to handle the discretization of catchments.
    """

    def __init__(self, catchment: Catchment) -> None:
        """
        Initialize the Discretization class.

        Parameters
        ----------
        catchment
            The catchment object.
        """
        self.catchment: Catchment = catchment

    def create_elevation_bands(
        self,
        method: str = "equal_intervals",
        number: int = 100,
        distance: int = 50,
        min_elevation: int | None = None,
        max_elevation: int | None = None,
        split_discontinuous: bool = False,
        min_patch_area: float | None = None,
        connectivity: int = 8,
    ) -> None:
        """
        Construction of the elevation bands based on the chosen method.

        Creates hydro units based on elevation bands using either equal-interval
        contours or quantile-based discretization. Results are stored in the
        catchment's map_unit_ids and hydro_units objects.

        Parameters
        ----------
        method
            The method to build the elevation bands:
            'equal_intervals' = fixed contour intervals (needs 'distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            needs 'number' parameter)
        number
            Number of bands to create for the 'quantiles' method.
        distance
            Distance (m) between the contour lines for the 'equal_intervals' method.
        min_elevation
            Minimum elevation of the elevation bands (to homogenize between runs).
        max_elevation
            Maximum elevation of the elevation bands (to homogenize between runs).
        split_discontinuous
            If True, a hydro unit made of several spatially disconnected patches is
            split into one hydro unit per patch. Default: False (a hydro unit can
            span spatially distant areas). Requires scipy.
        min_patch_area
            Minimum area (m2) for a disconnected patch to become its own hydro unit
            when 'split_discontinuous' is True. Smaller patches are merged into the
            largest retained patch of the same unit (no area is lost).
            Default: None = no minimum (every patch becomes a hydro unit).
        connectivity
            Pixel connectivity defining a patch when 'split_discontinuous' is True:
            8 = diagonal neighbours are connected (default), 4 = only the neighbours
            sharing an edge are connected.
        """
        self.discretize_by(
            "elevation",
            method,
            number,
            distance,
            min_elevation,
            max_elevation,
            split_discontinuous=split_discontinuous,
            min_patch_area=min_patch_area,
            connectivity=connectivity,
        )

    def discretize_by(
        self,
        criteria: str,
        elevation_method: str = "equal_intervals",
        elevation_number: int = 100,
        elevation_distance: int = 100,
        min_elevation: int | None = None,
        max_elevation: int | None = None,
        slope_method: str = "equal_intervals",
        slope_number: int = 6,
        slope_distance: int = 15,
        min_slope: int = 0,
        max_slope: int = 90,
        radiation_method: str = "equal_intervals",
        radiation_number: int = 5,
        radiation_distance: int = 50,
        min_radiation: int | None = None,
        max_radiation: int | None = None,
        sub_catchments: str | Path | None = None,
        split_discontinuous: bool = False,
        min_patch_area: float | None = None,
        connectivity: int = 8,
    ) -> None:
        """
        Construction of the elevation bands based on the chosen method.

        Discretizes the catchment into hydro units based on multiple criteria
        (elevation, slope, aspect, radiation, sub_catchments). Creates all
        combinations of specified criteria and populates the map_unit_ids and
        hydro_units objects.

        Parameters
        ----------
        criteria
            The criteria to use to discretize the catchment (can be combined):
            'elevation' = elevation bands
            'aspect' = aspect according to the cardinal directions (4 classes)
            'radiation' = potential radiation (Hock, 1999)
            'slope' = slope in degrees
            'sub_catchments' = membership to a sub-catchment polygon (keeps a hydro
            unit from spanning spatially-distant areas); requires the
            'sub_catchments' argument
        elevation_method
            The method to build the elevation bands:
            'equal_intervals' = fixed contour intervals
            (provide the 'elevation_distance' parameter)
            'quantiles' = quantiles of the catchment area
            (same surface; provide the 'elevation_number' parameter)
        elevation_number
            Number of elevation bands to create for the 'quantiles' method.
        elevation_distance
            Distance (m) between the contour lines for the 'equal_intervals' method.
        min_elevation
            Minimum elevation of the elevation bands (to homogenize between runs).
        max_elevation
            Maximum elevation of the elevation bands (to homogenize between runs).
        slope_method
            The method to build the slope categories:
            'equal_intervals' = fixed slope intervals (needs 'slope_distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'slope_number' parameter)
        slope_number
            Number of slope bands to create for the 'quantiles' method.
        slope_distance
            Distance (degrees) between the slope lines for the 'equal_intervals' method.
        min_slope
            Minimum slope of the slope bands (to homogenize between runs).
        max_slope
            Maximum slope of the slope bands (to homogenize between runs).
        radiation_method
            The method to build the radiation categories:
            'equal_intervals' = fixed radiation intervals
            (provide the 'radiation_distance' parameter)
            'quantiles' = quantiles of the catchment area
            (same surface; provide the 'radiation_number' parameter)
        radiation_number
            Number of radiation bands to create when using the 'quantiles' method.
        radiation_distance
            Distance (W/m2) in terms of radiation for the 'equal_intervals' method.
        min_radiation
            Minimum radiation of the radiation bands (to homogenize between runs).
        max_radiation
            Maximum radiation of the radiation bands (to homogenize between runs).
        sub_catchments
            Path to a vector file of sub-catchment polygons. Required when
            'sub_catchments' is in ``criteria``. Each polygon defines a sub-catchment;
            a DEM cell is assigned to the first polygon covering it. The sub-catchment
            polygons must cover the whole catchment: if any catchment cell is left
            uncovered, a ``DataError`` is raised (rather than silently dropping that
            area from the hydro units).
        split_discontinuous
            If True, a hydro unit made of several spatially disconnected patches is
            split into one hydro unit per patch. This is the geometry-driven
            alternative to the 'sub_catchments' criterion, and needs no additional
            data. Default: False (a hydro unit can span spatially distant areas).
            Requires scipy.
        min_patch_area
            Minimum area (m2) for a disconnected patch to become its own hydro unit
            when 'split_discontinuous' is True. Smaller patches are merged into the
            largest retained patch of the same unit, so no area is lost. If no patch
            of a unit reaches the threshold, the unit is left unsplit.
            Default: None = no minimum (every patch becomes a hydro unit).
        connectivity
            Pixel connectivity defining a patch when 'split_discontinuous' is True:
            8 = diagonal neighbours are connected (default), 4 = only the neighbours
            sharing an edge are connected.
        """
        if not HAS_PYPROJ:
            raise DependencyError(
                "pyproj is required for catchment discretization.",
                package_name="pyproj",
                operation="CatchmentDiscretization.create_hydro_units",
                install_command="pip install pyproj",
            )

        if split_discontinuous and not HAS_SCIPY:
            raise DependencyError(
                "scipy is required to split discontinuous hydro units.",
                package_name="scipy",
                operation="CatchmentDiscretization.discretize_by",
                install_command="pip install scipy",
            )

        if connectivity not in (4, 8):
            raise ConfigurationError(
                "The connectivity must be 4 or 8.",
                item_name="connectivity",
                item_value=connectivity,
                reason="Invalid pixel connectivity",
            )

        if min_patch_area is not None and min_patch_area <= 0:
            raise ConfigurationError(
                "The minimum patch area must be positive.",
                item_name="min_patch_area",
                item_value=min_patch_area,
                reason="Invalid minimum patch area",
            )

        if isinstance(criteria, str):
            criteria = [criteria]

        if (
            self.catchment.topography.slope is None
            or self.catchment.topography.aspect is None
        ):
            self.catchment.topography.calculate_slope_aspect()
        if (
            "radiation" in criteria
            and self.catchment.solar_radiation.mean_annual_radiation is None
        ):
            raise ModelError("Please first compute the radiation.")

        self.map_unit_ids = np.zeros(self.catchment.dem_data.shape)

        # Create a dict to store the criteria
        criteria_dict = {}

        if "elevation" in criteria:
            criteria_dict["elevation"] = []
            if elevation_method == "equal_intervals":
                dist = elevation_distance
                if min_elevation is None:
                    min_elevation = np.nanmin(self.catchment.dem_data)
                    min_elevation = min_elevation - (min_elevation % dist)
                if max_elevation is None:
                    max_elevation = np.nanmax(self.catchment.dem_data)
                    max_elevation = max_elevation + (dist - max_elevation % dist)
                elevations = np.arange(min_elevation, max_elevation + dist, dist)
                for i in range(len(elevations) - 1):
                    criteria_dict["elevation"].append(elevations[i : i + 2])
            elif elevation_method == "quantiles":
                elevations = np.nanquantile(
                    self.catchment.dem_data, np.linspace(0, 1, num=elevation_number)
                )
                for i in range(len(elevations) - 1):
                    criteria_dict["elevation"].append(elevations[i : i + 2])
            else:
                raise ConfigurationError(
                    "Unknown elevation band creation method.",
                    item_name="elevation_method",
                    item_value=elevation_method,
                    reason="Invalid method value",
                )

        if "slope" in criteria:
            criteria_dict["slope"] = []
            if slope_method == "equal_intervals":
                dist = slope_distance
                if min_slope is None:
                    min_slope = np.nanmin(self.catchment.topography.slope)
                    min_slope = min_slope - (min_slope % dist)
                if max_slope is None:
                    max_slope = np.nanmax(self.catchment.topography.slope)
                    max_slope = max_slope + (dist - max_slope % dist)
                slopes = np.arange(min_slope, max_slope + dist, dist)
                for i in range(len(slopes) - 1):
                    criteria_dict["slope"].append(slopes[i : i + 2])
            elif slope_method == "quantiles":
                slopes = np.nanquantile(
                    self.catchment.topography.slope, np.linspace(0, 1, num=slope_number)
                )
                for i in range(len(slopes) - 1):
                    criteria_dict["slope"].append(slopes[i : i + 2])
            else:
                raise ConfigurationError(
                    "Unknown slope band creation method.",
                    item_name="slope_method",
                    item_value=slope_method,
                    reason="Invalid method value",
                )

        if "aspect" in criteria:
            criteria_dict["aspect"] = ["N", "E", "S", "W"]

        if "radiation" in criteria:
            if self.catchment.solar_radiation.mean_annual_radiation is None:
                raise ConfigurationError(
                    "No radiation data available. Compute the radiation first.",
                    reason="Missing required radiation data",
                )
            criteria_dict["radiation"] = []
            if radiation_method == "equal_intervals":
                dist = radiation_distance
                if min_radiation is None:
                    min_radiation = np.nanmin(
                        self.catchment.solar_radiation.mean_annual_radiation
                    )
                    min_radiation = min_radiation - (min_radiation % dist)
                if max_radiation is None:
                    max_radiation = np.nanmax(
                        self.catchment.solar_radiation.mean_annual_radiation
                    )
                    max_radiation = max_radiation + (dist - max_radiation % dist)
                radiations = np.arange(min_radiation, max_radiation + dist, dist)
                for i in range(len(radiations) - 1):
                    criteria_dict["radiation"].append(radiations[i : i + 2])
            elif radiation_method == "quantiles":
                radiations = np.nanquantile(
                    self.catchment.solar_radiation.mean_annual_radiation,
                    np.linspace(0, 1, num=radiation_number),
                )
                for i in range(len(radiations) - 1):
                    criteria_dict["radiation"].append(radiations[i : i + 2])
            else:
                raise ConfigurationError(
                    "Unknown radiation band creation method. "
                    "Only 'equal_intervals' and 'quantiles' are supported.",
                    item_name="radiation_method",
                    item_value=radiation_method,
                    reason="Invalid method value",
                )

        sub_catchments_map = None
        if "sub_catchments" in criteria:
            if sub_catchments is None:
                raise ConfigurationError(
                    "The 'sub_catchments' criterion requires the 'sub_catchments' "
                    "argument (path to the sub-catchment polygons).",
                    item_name="sub_catchments",
                    item_value=None,
                    reason="Missing sub-catchment file",
                )
            sub_catchments_map, n_sub = self._build_sub_catchments_map(sub_catchments)
            criteria_dict["sub_catchments"] = list(range(1, n_sub + 1))

        res_elevation = []
        res_elevation_min = []
        res_elevation_max = []
        res_slope = []
        res_slope_min = []
        res_slope_max = []
        res_aspect_class = []
        res_radiation = []
        res_radiation_min = []
        res_radiation_max = []
        res_sub_catchment = []

        combinations = list(itertools.product(*criteria_dict.values()))
        combinations_keys = list(criteria_dict.keys())
        # Position of each criterion within a combination tuple (computed once).
        criterion_positions = {name: pos for pos, name in enumerate(combinations_keys)}

        unit_id = 1
        for combination in combinations:
            mask_unit = np.ones(self.catchment.dem_data.shape, dtype=bool)
            # Mask nan values
            mask_unit[np.isnan(self.catchment.dem_data)] = False

            for criterion_name, criterion in zip(combinations_keys, combination):
                if criterion_name == "elevation":
                    mask_elev = np.logical_and(
                        self.catchment.dem_data >= criterion[0],
                        self.catchment.dem_data < criterion[1],
                    )
                    mask_unit = np.logical_and(mask_unit, mask_elev)
                elif criterion_name == "slope":
                    mask_slope = np.logical_and(
                        self.catchment.topography.slope >= criterion[0],
                        self.catchment.topography.slope < criterion[1],
                    )
                    mask_unit = np.logical_and(mask_unit, mask_slope)
                elif criterion_name == "aspect":
                    if criterion == "N":
                        mask_aspect = np.logical_or(
                            np.logical_and(
                                self.catchment.topography.aspect >= 315,
                                self.catchment.topography.aspect <= 360,
                            ),
                            np.logical_and(
                                self.catchment.topography.aspect >= 0,
                                self.catchment.topography.aspect < 45,
                            ),
                        )
                    elif criterion == "E":
                        mask_aspect = np.logical_and(
                            self.catchment.topography.aspect >= 45,
                            self.catchment.topography.aspect < 135,
                        )
                    elif criterion == "S":
                        mask_aspect = np.logical_and(
                            self.catchment.topography.aspect >= 135,
                            self.catchment.topography.aspect < 225,
                        )
                    elif criterion == "W":
                        mask_aspect = np.logical_and(
                            self.catchment.topography.aspect >= 225,
                            self.catchment.topography.aspect < 315,
                        )
                    else:
                        raise ConfigurationError(
                            "Unknown aspect value.",
                            item_name="aspect",
                            item_value=criterion,
                            reason="Invalid aspect direction",
                        )
                    mask_unit = np.logical_and(mask_unit, mask_aspect)
                elif criterion_name == "radiation":
                    radiation = self.catchment.solar_radiation.mean_annual_radiation
                    mask_radiation = np.logical_and(
                        radiation >= criterion[0], radiation < criterion[1]
                    )
                    mask_unit = np.logical_and(mask_unit, mask_radiation)
                elif criterion_name == "sub_catchments":
                    mask_sub = sub_catchments_map == criterion
                    mask_unit = np.logical_and(mask_unit, mask_sub)

            # If the unit is empty, skip it
            if np.count_nonzero(mask_unit) == 0:
                continue

            # Check that all cells in unit_ids are 0
            assert np.count_nonzero(self.map_unit_ids[mask_unit]) == 0

            # Set the unit id
            self.map_unit_ids[mask_unit] = unit_id

            # Set the mean elevation of the unit if elevation is a criterion
            if "elevation" in criteria_dict.keys():
                elevations = combination[criterion_positions["elevation"]]
                res_elevation.append(round(float(np.mean(elevations)), 2))
                res_elevation_min.append(round(float(elevations[0]), 2))
                res_elevation_max.append(round(float(elevations[1]), 2))

            # Set the mean slope of the unit if slope is a criterion
            if "slope" in criteria_dict.keys():
                slopes = combination[criterion_positions["slope"]]
                res_slope.append(round(float(np.mean(slopes)), 2))
                res_slope_min.append(round(float(slopes[0]), 2))
                res_slope_max.append(round(float(slopes[1]), 2))

            # Get the aspect class if aspect is a criterion
            if "aspect" in criteria_dict.keys():
                res_aspect_class.append(combination[criterion_positions["aspect"]])

            # Get the radiation class if radiation is a criterion
            if "radiation" in criteria_dict.keys():
                radiations = combination[criterion_positions["radiation"]]
                res_radiation.append(round(float(np.mean(radiations)), 2))
                res_radiation_min.append(round(float(radiations[0]), 2))
                res_radiation_max.append(round(float(radiations[1]), 2))

            # Get the sub-catchment id if sub_catchments is a criterion
            if "sub_catchments" in criteria_dict.keys():
                sub_id = combination[criterion_positions["sub_catchments"]]
                res_sub_catchment.append(str(sub_id))

            unit_id += 1

        if split_discontinuous:
            self.map_unit_ids, origins = self._split_discontinuous_units(
                self.map_unit_ids, unit_id - 1, connectivity, min_patch_area
            )

            # The criteria values were collected per original unit: replicate them
            # for each of the new units issued from a split.
            def _reindex(values: list) -> list:
                return [values[i] for i in origins] if values else values

            res_elevation = _reindex(res_elevation)
            res_elevation_min = _reindex(res_elevation_min)
            res_elevation_max = _reindex(res_elevation_max)
            res_slope = _reindex(res_slope)
            res_slope_min = _reindex(res_slope_min)
            res_slope_max = _reindex(res_slope_max)
            res_aspect_class = _reindex(res_aspect_class)
            res_radiation = _reindex(res_radiation)
            res_radiation_min = _reindex(res_radiation_min)
            res_radiation_max = _reindex(res_radiation_max)
            res_sub_catchment = _reindex(res_sub_catchment)

        # The unit ids are stored as uint16 in the raster: fail rather than wrap.
        n_units = int(self.map_unit_ids.max())
        max_units = int(np.iinfo(np.uint16).max)
        if n_units > max_units:
            raise DataError(
                f"The discretization produced {n_units} hydro units, which exceeds "
                f"the maximum of {max_units}. Use coarser criteria or a larger "
                f"'min_patch_area'.",
                data_type="hydro units",
                reason="Too many hydro units",
            )

        self.catchment.map_unit_ids = self.map_unit_ids.astype(rasterio.uint16)

        if res_elevation:
            self.catchment.hydro_units.add_property(("elevation", "m"), res_elevation)
            self.catchment.hydro_units.add_property(
                ("elevation_min", "m"), res_elevation_min
            )
            self.catchment.hydro_units.add_property(
                ("elevation_max", "m"), res_elevation_max
            )

        # Only the band bounds are stored here: the mean slope of every unit is
        # always computed from the DEM by get_hydro_units_attributes (the lateral
        # processes, e.g. the snow redistribution, need it whether or not the
        # discretization used the slope).
        if res_slope:
            self.catchment.hydro_units.add_property(("slope_min", "deg"), res_slope_min)
            self.catchment.hydro_units.add_property(("slope_max", "deg"), res_slope_max)

        if res_aspect_class:
            self.catchment.hydro_units.add_property(
                ("aspect_class", "-"), res_aspect_class
            )

        if res_radiation:
            self.catchment.hydro_units.add_property(
                ("radiation", "W/m2"), res_radiation
            )
            self.catchment.hydro_units.add_property(
                ("radiation_min", "W/m2"), res_radiation_min
            )
            self.catchment.hydro_units.add_property(
                ("radiation_max", "W/m2"), res_radiation_max
            )

        if res_sub_catchment:
            self.catchment.hydro_units.add_property(
                ("sub_catchment", "-"), res_sub_catchment
            )

        self.catchment.initialize_land_cover_fractions()
        self.catchment.get_hydro_units_attributes()
        self.catchment.hydro_units.populate_bounded_instance()

    def _split_discontinuous_units(
        self,
        map_unit_ids: np.ndarray,
        n_units: int,
        connectivity: int = 8,
        min_patch_area: float | None = None,
    ) -> tuple[np.ndarray, list[int]]:
        """Split spatially discontinuous hydro units into one unit per patch.

        Hydro units are defined by combinations of criteria (elevation, aspect, ...),
        so a single unit usually covers several spatially disjoint patches. This
        relabels the unit id map so that each connected patch becomes its own hydro
        unit. Patches smaller than ``min_patch_area`` are not promoted: they are
        merged into the largest retained patch of the same original unit, so that no
        area is ever lost. If no patch of a unit reaches the threshold, the unit is
        left untouched (i.e. a single, discontinuous unit).

        Parameters
        ----------
        map_unit_ids
            The per-cell hydro unit id map (0 = outside the catchment).
        n_units
            The number of hydro units in ``map_unit_ids`` (ids 1..n_units).
        connectivity
            Pixel connectivity defining a patch: 8 (diagonals count) or 4.
        min_patch_area
            Minimum area (m2) for a disconnected patch to become its own hydro unit.
            None = no minimum (every patch becomes a hydro unit).

        Returns
        -------
        A tuple ``(new_map, origins)`` with the relabelled id map (dense ids 1..M,
        grouped by original unit) and, for each new unit, the 0-based index of the
        original unit it originates from.
        """
        structure = ndimage.generate_binary_structure(2, 2 if connectivity == 8 else 1)

        min_cells = 1
        if min_patch_area is not None:
            px_area = self.catchment.get_dem_pixel_area()
            min_cells = max(1, int(np.ceil(min_patch_area / px_area)))

        new_map = np.zeros_like(map_unit_ids)
        origins: list[int] = []
        new_id = 0

        for old_id in range(1, n_units + 1):
            mask_unit = map_unit_ids == old_id
            labels, n_patches = ndimage.label(mask_unit, structure=structure)

            if n_patches <= 1:
                new_id += 1
                origins.append(old_id - 1)
                new_map[mask_unit] = new_id
                continue

            # Patch sizes in cells (the first bin is the background).
            sizes = np.bincount(labels.ravel())[1:]
            # Patches large enough to become their own unit, largest first.
            kept = [p for p in np.argsort(-sizes) + 1 if sizes[p - 1] >= min_cells]

            if not kept:
                # No patch is large enough: keep the unit whole, as it would be
                # without splitting.
                new_id += 1
                origins.append(old_id - 1)
                new_map[mask_unit] = new_id
                continue

            largest_new_id = new_id + 1
            for patch in kept:
                new_id += 1
                origins.append(old_id - 1)
                new_map[labels == patch] = new_id

            # Merge the patches below the threshold into the largest retained patch.
            if len(kept) < n_patches:
                too_small = mask_unit & np.isin(labels, kept, invert=True)
                new_map[too_small] = largest_new_id

        return new_map, origins

    def _build_sub_catchments_map(
        self, sub_catchments: str | Path
    ) -> tuple[np.ndarray, int]:
        """Rasterize sub-catchment polygons onto the DEM grid.

        Reads the sub-catchment polygons, reprojects them to the catchment CRS and
        assigns each DEM cell to the first polygon covering it (ids 1..N in feature
        order). The sub-catchment polygons must cover the whole catchment: if any
        catchment cell is left uncovered, a ``DataError`` is raised (rather than
        silently dropping that area from the hydro units).

        Parameters
        ----------
        sub_catchments
            Path to the vector file of sub-catchment polygons.

        Returns
        -------
        A tuple ``(sub_catchments_map, n_sub)`` with the per-cell integer id grid over
        the DEM and the number of sub-catchments.

        Raises
        ------
        DataError
            If part of the catchment is not covered by any sub-catchment polygon.
        """
        if not HAS_GEOPANDAS:
            raise DependencyError(
                "geopandas is required to discretize by sub-catchments.",
                package_name="geopandas",
                operation="CatchmentDiscretization.discretize_by",
                install_command="pip install geopandas",
            )
        gdf = gpd.read_file(sub_catchments)
        gdf = gdf.to_crs(self.catchment.crs)
        if gdf.empty:
            raise DataError(
                "The sub-catchments file contains no polygons.",
                data_type="sub-catchments",
                reason="Empty file",
            )

        sub_map = np.zeros(self.catchment.dem_data.shape, dtype=int)
        assigned = np.zeros(sub_map.shape, dtype=bool)
        for i in range(len(gdf)):
            subset = gdf.iloc[[i]]
            masked = self.catchment.mask_dem(subset, nodata=-9999, all_touched=True)
            present = (masked != -9999) & ~assigned
            sub_map[present] = i + 1
            assigned |= present

        # Ensure the sub-catchments cover the whole catchment. Catchment cells are
        # those with a valid elevation; any such cell left with id 0 would be silently
        # dropped from the hydro units, so fail instead.
        inside_catchment = ~np.isnan(self.catchment.dem_data)
        uncovered = inside_catchment & (sub_map == 0)
        n_uncovered = int(np.count_nonzero(uncovered))
        if n_uncovered > 0:
            uncovered_area = n_uncovered * self.catchment.get_dem_pixel_area()
            raise DataError(
                f"The sub-catchment polygons do not cover the whole catchment: "
                f"{n_uncovered} cell(s) ({uncovered_area:.0f} m²) are left uncovered. "
                f"Provide sub-catchments that tile the entire catchment.",
                data_type="sub-catchments",
                reason="Incomplete catchment coverage",
            )

        return sub_map, len(gdf)
