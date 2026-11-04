from __future__ import annotations

import heapq
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
        min_unit_area: float | None = None,
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
        min_unit_area
            Minimum area (m2) of a hydro unit: smaller units are merged into a
            neighbouring unit (see :meth:`discretize_by`). Default: None = no
            minimum.
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
            min_unit_area=min_unit_area,
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
        aspect_classes: int = 4,
        flat_slope_threshold: float = 3.0,
        radiation_method: str = "equal_intervals",
        radiation_number: int = 5,
        radiation_distance: int = 50,
        min_radiation: int | None = None,
        max_radiation: int | None = None,
        sub_catchments: str | Path | None = None,
        split_discontinuous: bool = False,
        min_patch_area: float | None = None,
        connectivity: int = 8,
        min_unit_area: float | None = None,
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
            'aspect' = aspect classes (see the 'aspect_classes' parameter)
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
        aspect_classes
            Number of aspect classes for the 'aspect' criterion: 4 (the cardinal
            directions N, E, S, W; default) or 8 (N, NE, E, SE, S, SW, W, NW). The
            sectors are centred on their direction, so N spans 337.5-22.5° with 8
            classes and 315-45° with 4.
        flat_slope_threshold
            Slope (degrees) below which a cell is considered flat and assigned to the
            'FLAT' aspect class rather than to a direction (default: 3). Cells whose
            aspect is undefined (flat terrain, or the border of the DEM) are always
            assigned to 'FLAT', whatever the threshold, so that no area is lost. The
            'FLAT' class is only created when such cells exist; 0 keeps only the cells
            with an undefined aspect (PREVAH uses 0.5).

            The default suits a 25 m DEM with about a metre of vertical error, where
            below 3° the aspect of a cell is mostly noise: perturbing the DEM by 1 m
            moves 37 % of the cells of the 1-2° band to another aspect class out of
            four (a uniformly random aspect would move 75 %), against 14 % at 3-5°.
            Little is lost by not splitting them, since the north-south difference in
            annual clear-sky radiation at 3° is under 8 % of the horizontal. The error
            of the aspect scales with the vertical error over the cell size, so a finer
            DEM calls for a *higher* threshold, not a lower one:
            ``atan(1.1 * sigma_z / cell_size)`` holds the median error of the aspect
            near 15°.
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
        min_unit_area
            Minimum area (m2) of a hydro unit. A smaller unit is merged into a unit
            it borders within the same sub-catchment and subbasin, never across:
            the one in the closest elevation band, and among those the one sharing
            the longest border. Units are merged smallest first, until each one
            reaches the threshold or has no neighbour left in its sub-catchment.
            The receiving unit keeps its criteria values (e.g. its elevation band),
            while its area, mean elevation, slope and aspect are computed from all
            its cells. Combined with 'sub_catchments', this gives few hydro units
            whose parts all lie within one sub-catchment. The merge comes before
            'split_discontinuous'. Default: None = no minimum.

        Notes
        -----
        When the subbasins were delineated beforehand
        (:meth:`Catchment.delineate_subbasins
        <hydrobricks.Catchment.delineate_subbasins>`), they are an implicit criterion:
        every hydro unit lies within a single subbasin, the units carry the
        ``subbasin`` column and the subbasin table is set on them.
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

        if min_unit_area is not None and min_unit_area <= 0:
            raise ConfigurationError(
                "The minimum unit area must be positive.",
                item_name="min_unit_area",
                item_value=min_unit_area,
                reason="Invalid minimum unit area",
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

        aspect_masks = {}
        if "aspect" in criteria:
            aspect_masks = self._build_aspect_masks(
                aspect_classes, flat_slope_threshold
            )
            criteria_dict["aspect"] = list(aspect_masks.keys())

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

        # Subbasins delineated beforehand: no hydro unit may span two of them.
        subbasin_map = self.catchment.map_subbasin_ids
        if subbasin_map is not None:
            criteria_dict["subbasin"] = [
                int(i) for i in np.unique(subbasin_map[subbasin_map > 0])
            ]

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
        res_subbasin = []

        combinations_keys = list(criteria_dict.keys())
        # Position of each criterion within a combination tuple (computed once).
        criterion_positions = {name: pos for pos, name in enumerate(combinations_keys)}

        # The class of every cell for each criterion (-1 = none), and from them the
        # combination of every cell, numbered in the order of
        # itertools.product(*criteria_dict.values()): the hydro units are the
        # non-empty combinations, in that order.
        class_maps = [
            self._criterion_class_map(
                name, values, aspect_masks, sub_catchments_map, subbasin_map
            )
            for name, values in criteria_dict.items()
        ]
        n_classes = [len(values) for values in criteria_dict.values()]
        valid = ~np.isnan(self.catchment.dem_data)
        for class_map in class_maps:
            valid &= class_map >= 0
        if class_maps:
            cell_codes = np.ravel_multi_index(
                [class_map[valid] for class_map in class_maps], n_classes
            )
        else:  # no criterion: a single unit
            cell_codes = np.zeros(np.count_nonzero(valid), dtype=int)
        unit_codes, cell_units = np.unique(cell_codes, return_inverse=True)
        self.map_unit_ids[valid] = cell_units + 1
        unit_classes = np.unravel_index(unit_codes, n_classes)
        criteria_values = list(criteria_dict.values())

        unit_id = 1
        for i_unit in range(len(unit_codes)):
            combination = tuple(
                values[classes[i_unit]]
                for values, classes in zip(criteria_values, unit_classes)
            )

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

            if "subbasin" in criteria_dict.keys():
                res_subbasin.append(combination[criterion_positions["subbasin"]])

            unit_id += 1

        res_lists = [
            res_elevation,
            res_elevation_min,
            res_elevation_max,
            res_slope,
            res_slope_min,
            res_slope_max,
            res_aspect_class,
            res_radiation,
            res_radiation_min,
            res_radiation_max,
            res_sub_catchment,
            res_subbasin,
        ]

        def _reindex(origins: list[int]) -> None:
            # The criteria values were collected per original unit: take them from
            # the unit each new one originates from.
            for values in res_lists:
                if values:
                    values[:] = [values[i] for i in origins]

        if min_unit_area is not None:
            n_units = unit_id - 1
            # A unit is never merged out of its sub-catchment or subbasin.
            containers = list(
                zip(
                    res_sub_catchment or [None] * n_units,
                    res_subbasin or [None] * n_units,
                )
            )
            if "elevation" in criteria_dict:
                levels = unit_classes[criterion_positions["elevation"]].tolist()
            else:
                levels = [0] * n_units
            self.map_unit_ids, origins = self._merge_small_units(
                self.map_unit_ids, n_units, min_unit_area, containers, levels
            )
            _reindex(origins)

        if split_discontinuous:
            self.map_unit_ids, origins = self._split_discontinuous_units(
                self.map_unit_ids,
                int(self.map_unit_ids.max()),
                connectivity,
                min_patch_area,
            )
            _reindex(origins)

        # The unit ids are stored as uint16 in the raster: fail rather than wrap.
        n_units = int(self.map_unit_ids.max())
        max_units = int(np.iinfo(np.uint16).max)
        if n_units > max_units:
            raise DataError(
                f"The discretization produced {n_units} hydro units, which exceeds "
                f"the maximum of {max_units}. Use coarser criteria, a larger "
                f"'min_patch_area' or a 'min_unit_area'.",
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

        if res_subbasin:
            hydro_units = self.catchment.hydro_units
            column = (hydro_units.SUBBASIN_COLUMN, "-")
            if hydro_units.has(hydro_units.SUBBASIN_COLUMN):
                hydro_units.hydro_units[column] = res_subbasin
            else:
                hydro_units.add_property(column, res_subbasin)
            if self.catchment.network.table is not None:
                hydro_units.set_subbasins(self.catchment.network.table)

        self.catchment.initialize_land_cover_fractions()
        self.catchment.get_hydro_units_attributes()
        self.catchment.hydro_units.populate_bounded_instance()

    def _criterion_class_map(
        self,
        name: str,
        values: list,
        aspect_masks: dict[str, np.ndarray],
        sub_catchments_map: np.ndarray | None,
        subbasin_map: np.ndarray | None,
    ) -> np.ndarray:
        """The class of every DEM cell for one discretization criterion.

        Parameters
        ----------
        name
            The criterion name.
        values
            Its classes, as listed in the criteria dict: the ``[min, max)`` bounds
            of each band for the elevation, slope and radiation, the class names for
            the aspect, the ids for the sub-catchments and subbasins.
        aspect_masks
            The cell mask of every aspect class.
        sub_catchments_map
            The per-cell sub-catchment id (1..N), for the 'sub_catchments' criterion.
        subbasin_map
            The per-cell subbasin id, for the 'subbasin' criterion.

        Returns
        -------
        The index of the class of every cell in ``values``, -1 where none applies.
        """
        shape = self.catchment.dem_data.shape
        if name in ("elevation", "slope", "radiation"):
            data = {
                "elevation": self.catchment.dem_data,
                "slope": self.catchment.topography.slope,
                "radiation": self.catchment.solar_radiation.mean_annual_radiation,
            }[name]
            # The bands are contiguous: band i is [edges[i], edges[i + 1]).
            edges = np.array([values[0][0]] + [band[1] for band in values])
            with np.errstate(invalid="ignore"):
                classes = np.searchsorted(edges, data, side="right") - 1
                classes[~(data >= edges[0]) | ~(data < edges[-1])] = -1
            return classes
        classes = np.full(shape, -1)
        if name == "aspect":
            for i, aspect_class in enumerate(values):
                classes[aspect_masks[aspect_class]] = i
        elif name == "sub_catchments":
            inside = sub_catchments_map > 0
            classes[inside] = sub_catchments_map[inside] - 1
        elif name == "subbasin":
            ids = np.asarray(values)
            inside = np.isin(subbasin_map, ids)
            classes[inside] = np.searchsorted(ids, subbasin_map[inside])
        return classes

    def _merge_small_units(
        self,
        map_unit_ids: np.ndarray,
        n_units: int,
        min_unit_area: float,
        containers: list,
        levels: list[int],
    ) -> tuple[np.ndarray, list[int]]:
        """Merge the hydro units smaller than a minimum area into a neighbour.

        The smallest unit below the threshold is merged into one of the units it
        shares a border with (an edge between two cells) within the same container
        (sub-catchment and subbasin), and so on until every unit reaches the
        threshold. The receiving unit is the one of the closest level (the
        elevation band), and among those the one sharing the longest border. It
        keeps its own attributes. A unit with no neighbour in its container is
        left as it is.

        Parameters
        ----------
        map_unit_ids
            The per-cell hydro unit id map (0 = outside the catchment).
        n_units
            The number of hydro units in ``map_unit_ids`` (ids 1..n_units).
        min_unit_area
            Minimum area (m2) of a hydro unit.
        containers
            For each unit, the key of the area it may not leave: two units are only
            merged when their keys are equal.
        levels
            For each unit, its level (e.g. the index of its elevation band): a unit
            is preferably merged into a neighbour of a close level.

        Returns
        -------
        A tuple ``(new_map, origins)`` with the relabelled id map (dense ids 1..M,
        in the order of the original ids) and, for each new unit, the 0-based index
        of the original unit it originates from.
        """
        px_area = self.catchment.get_dem_pixel_area()
        min_cells = max(1, int(np.ceil(min_unit_area / px_area)))

        ids = map_unit_ids.astype(np.int64)
        sizes = np.bincount(ids.ravel(), minlength=n_units + 1)

        # The borders between units: the number of cell edges they share.
        first = np.concatenate([ids[:, :-1].ravel(), ids[:-1, :].ravel()])
        second = np.concatenate([ids[:, 1:].ravel(), ids[1:, :].ravel()])
        between = (first != second) & (first > 0) & (second > 0)
        low = np.minimum(first[between], second[between])
        high = np.maximum(first[between], second[between])
        keys, counts = np.unique(low * (n_units + 1) + high, return_counts=True)
        borders: list[dict[int, int]] = [{} for _ in range(n_units + 1)]
        for key, count in zip(keys.tolist(), counts.tolist()):
            u, v = divmod(key, n_units + 1)
            if containers[u - 1] == containers[v - 1]:
                borders[u][v] = count
                borders[v][u] = count

        target = np.arange(n_units + 1)
        queue = [(int(sizes[u]), u) for u in range(1, n_units + 1)]
        queue = [item for item in queue if item[0] < min_cells]
        heapq.heapify(queue)
        while queue:
            size, u = heapq.heappop(queue)
            if target[u] != u or size != sizes[u] or not borders[u]:
                continue  # merged already, grown since, or alone in its container
            v = min(
                borders[u],
                key=lambda w: (abs(levels[w - 1] - levels[u - 1]), -borders[u][w], w),
            )
            # Merge u into v: v takes over the cells and the borders of u.
            sizes[v] += sizes[u]
            sizes[u] = 0
            target[u] = v
            for w, count in borders[u].items():
                del borders[w][u]
                if w != v:
                    borders[v][w] = borders[v].get(w, 0) + count
                    borders[w][v] = borders[w].get(v, 0) + count
            borders[u] = {}
            if sizes[v] < min_cells:
                heapq.heappush(queue, (int(sizes[v]), v))

        # Follow the chains of merges to the final unit, then renumber densely.
        for u in range(1, n_units + 1):
            root = u
            while target[root] != root:
                root = target[root]
            target[u] = root
        survivors = [u for u in range(1, n_units + 1) if target[u] == u]
        new_ids = np.zeros(n_units + 1, dtype=np.int64)
        new_ids[survivors] = np.arange(1, len(survivors) + 1)
        new_map = new_ids[target][ids].astype(map_unit_ids.dtype)
        origins = [u - 1 for u in survivors]

        return new_map, origins

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

    def _build_aspect_masks(
        self, aspect_classes: int, flat_slope_threshold: float
    ) -> dict[str, np.ndarray]:
        """Build the cell mask of every aspect class.

        The sectors are centred on their direction (N spans 315-45° with 4 classes,
        337.5-22.5° with 8). Cells whose aspect is undefined -- flat terrain, which
        the aspect computation marks with -1, and the border of the DEM, where it is
        NaN -- belong to no sector, and so does a cell flatter than
        ``flat_slope_threshold``. They are gathered in a 'FLAT' class so that their
        area is not dropped from the catchment. That class is only created when such
        cells exist, so a catchment without them keeps the same hydro units as before.

        Parameters
        ----------
        aspect_classes
            Number of aspect classes: 4 or 8.
        flat_slope_threshold
            Slope (degrees) below which a cell is considered flat.

        Returns
        -------
        The mask of each class, keyed by class name, in clockwise order from north.

        Raises
        ------
        ConfigurationError
            If the number of aspect classes is not 4 or 8.
        """
        if aspect_classes == 4:
            names = ["N", "E", "S", "W"]
        elif aspect_classes == 8:
            names = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
        else:
            raise ConfigurationError(
                "The number of aspect classes must be 4 or 8.",
                item_name="aspect_classes",
                item_value=aspect_classes,
                reason="Invalid number of aspect classes",
            )

        aspect = self.catchment.topography.aspect
        slope = self.catchment.topography.slope

        # Undefined aspect (flat cells, marked -1, and NaN at the border of the DEM)
        # and, if asked for, the cells too flat for their aspect to mean anything.
        with np.errstate(invalid="ignore"):
            mask_flat = ~(aspect >= 0)
            if flat_slope_threshold > 0:
                mask_flat = np.logical_or(mask_flat, slope < flat_slope_threshold)

        width = 360 / aspect_classes
        masks = {}
        for i, name in enumerate(names):
            start = (i * width - width / 2) % 360
            end = start + width
            if end <= 360:
                mask = np.logical_and(aspect >= start, aspect < end)
            else:  # the north sector wraps around 0°
                mask = np.logical_or(aspect >= start, aspect < end - 360)
            masks[name] = np.logical_and(mask, ~mask_flat)

        # Outside the catchment the aspect is undefined too, so only the flat cells
        # that carry an elevation decide whether the class is needed.
        if np.any(np.logical_and(mask_flat, ~np.isnan(self.catchment.dem_data))):
            masks["FLAT"] = mask_flat

        return masks

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
