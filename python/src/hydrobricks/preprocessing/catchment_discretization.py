from __future__ import annotations

import itertools
from typing import TYPE_CHECKING

import numpy as np

import hydrobricks as hb

if TYPE_CHECKING:
    from hydrobricks.catchment import Catchment


class CatchmentDiscretization:
    """
    Class to handle the discretization of catchments.
    """

    def __init__(self, catchment: Catchment):
        """
        Initialize the Discretization class.

        Parameters
        ----------
        catchment
            The catchment object.
        """
        self.catchment = catchment

    def create_elevation_bands(
            self,
            method: str = 'equal_intervals',
            number: int = 100,
            distance: int = 50,
            min_elevation: int | None = None,
            max_elevation: int | None = None
    ):
        """
        Construction of the elevation bands based on the chosen method.

        Parameters
        ----------
        method
            The method to build the elevation bands:
            'equal_intervals' = fixed contour intervals (provide the 'distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'number' parameter)
        number
            Number of bands to create when using the 'quantiles' method.
        distance
            Distance (m) between the contour lines when using the 'equal_intervals' method.
        min_elevation
            Minimum elevation of the elevation bands (to homogenize between runs).
        max_elevation
            Maximum elevation of the elevation bands (to homogenize between runs).
        """
        self.discretize_by(
            'elevation',
            method,
            number,
            distance,
            min_elevation,
            max_elevation
        )

    def discretize_by(
            self,
            criteria: str,
            elevation_method: str = 'equal_intervals',
            elevation_number: int = 100,
            elevation_distance: int = 100,
            min_elevation: int | None = None,
            max_elevation: int | None = None,
            slope_method: str = 'equal_intervals',
            slope_number: int = 6,
            slope_distance: int = 15,
            min_slope: int = 0,
            max_slope: int = 90,
            radiation_method: str = 'equal_intervals',
            radiation_number: int = 5,
            radiation_distance: int = 50,
            min_radiation: int | None = None,
            max_radiation: int | None = None):
        """
        Construction of the elevation bands based on the chosen method.

        Parameters
        ----------
        criteria
            The criteria to use to discretize the catchment (can be combined):
            'elevation' = elevation bands
            'aspect' = aspect according to the cardinal directions (4 classes)
            'radiation' = potential radiation (Hock, 1999)
            'slope' = slope in degrees
        elevation_method
            The method to build the elevation bands:
            'equal_intervals' = fixed contour intervals (provide the 'elevation_distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'elevation_number' parameter)
        elevation_number
            Number of elevation bands to create when using the 'quantiles' method.
        elevation_distance
            Distance (m) between the contour lines when using the 'equal_intervals' method.
        min_elevation
            Minimum elevation of the elevation bands (to homogenize between runs).
        max_elevation
            Maximum elevation of the elevation bands (to homogenize between runs).
        slope_method
            The method to build the slope categories:
            'equal_intervals' = fixed slope intervals (provide the 'slope_distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'slope_number' parameter)
        slope_number
            Number of slope bands to create when using the 'quantiles' method.
        slope_distance
            Distance (degrees) between the slope lines when using the 'equal_intervals' method.
        min_slope
            Minimum slope of the slope bands (to homogenize between runs).
        max_slope
            Maximum slope of the slope bands (to homogenize between runs).
        radiation_method
            The method to build the radiation categories:
            'equal_intervals' = fixed radiation intervals (provide the 'radiation_distance' parameter)
            'quantiles' = quantiles of the catchment area (same surface;
            provide the 'radiation_number' parameter)
        radiation_number
            Number of radiation bands to create when using the 'quantiles' method.
        radiation_distance
            Distance (W/m2) between the radiation lines for the 'equal_intervals' method.
        min_radiation
            Minimum radiation of the radiation bands (to homogenize between runs).
        max_radiation
            Maximum radiation of the radiation bands (to homogenize between runs).
        """
        if not hb.has_pyproj:
            raise ImportError("pyproj is required to do this.")

        if isinstance(criteria, str):
            criteria = [criteria]

        if self.catchment.topography.slope is None or self.catchment.topography.aspect is None:
            self.catchment.topography.calculate_slope_aspect()
        if 'radiation' in criteria and self.catchment.solar_radiation.mean_annual_radiation is None:
            raise RuntimeError("Please first compute the radiation.")

        self.map_unit_ids = np.zeros(self.catchment.dem_data.shape)

        # Create a dict to store the criteria
        criteria_dict = {}

        if 'elevation' in criteria:
            criteria_dict['elevation'] = []
            if elevation_method == 'equal_intervals':
                dist = elevation_distance
                if min_elevation is None:
                    min_elevation = np.nanmin(self.catchment.dem_data)
                    min_elevation = min_elevation - (min_elevation % dist)
                if max_elevation is None:
                    max_elevation = np.nanmax(self.catchment.dem_data)
                    max_elevation = max_elevation + (dist - max_elevation % dist)
                elevations = np.arange(min_elevation, max_elevation + dist, dist)
                for i in range(len(elevations) - 1):
                    criteria_dict['elevation'].append(elevations[i:i + 2])
            elif elevation_method == 'quantiles':
                elevations = np.nanquantile(
                    self.catchment.dem_data,
                    np.linspace(0, 1, num=elevation_number))
                for i in range(len(elevations) - 1):
                    criteria_dict['elevation'].append(elevations[i:i + 2])
            else:
                raise ValueError("Unknown elevation band creation method.")

        if 'slope' in criteria:
            criteria_dict['slope'] = []
            if slope_method == 'equal_intervals':
                dist = slope_distance
                if min_slope is None:
                    min_slope = np.nanmin(self.catchment.topography.slope)
                    min_slope = min_slope - (min_slope % dist)
                if max_slope is None:
                    max_slope = np.nanmax(self.catchment.topography.slope)
                    max_slope = max_slope + (dist - max_slope % dist)
                slopes = np.arange(min_slope, max_slope + dist, dist)
                for i in range(len(slopes) - 1):
                    criteria_dict['slope'].append(slopes[i:i + 2])
            elif slope_method == 'quantiles':
                slopes = np.nanquantile(self.catchment.topography.slope,
                                        np.linspace(0, 1, num=slope_number))
                for i in range(len(slopes) - 1):
                    criteria_dict['slope'].append(slopes[i:i + 2])
            else:
                raise ValueError("Unknown slope band creation method.")

        if 'aspect' in criteria:
            criteria_dict['aspect'] = ['N', 'E', 'S', 'W']

        if 'radiation' in criteria:
            if self.catchment.solar_radiation.mean_annual_radiation is None:
                raise ValueError("No radiation data available. "
                                 "Compute the radiation first")
            criteria_dict['radiation'] = []
            if radiation_method == 'equal_intervals':
                dist = radiation_distance
                if min_radiation is None:
                    min_radiation = np.nanmin(
                        self.catchment.solar_radiation.mean_annual_radiation)
                    min_radiation = min_radiation - (min_radiation % dist)
                if max_radiation is None:
                    max_radiation = np.nanmax(
                        self.catchment.solar_radiation.mean_annual_radiation)
                    max_radiation = max_radiation + (dist - max_radiation % dist)
                radiations = np.arange(min_radiation, max_radiation + dist, dist)
                for i in range(len(radiations) - 1):
                    criteria_dict['radiation'].append(radiations[i:i + 2])
            elif radiation_method == 'quantiles':
                radiations = np.nanquantile(
                    self.catchment.solar_radiation.mean_annual_radiation,
                    np.linspace(0, 1, num=radiation_number))
                for i in range(len(radiations) - 1):
                    criteria_dict['radiation'].append(radiations[i:i + 2])
            else:
                raise ValueError("Unknown radiation band creation method.")

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

        combinations = list(itertools.product(*criteria_dict.values()))
        combinations_keys = criteria_dict.keys()

        unit_id = 1
        for i, criteria in enumerate(combinations):
            mask_unit = np.ones(self.catchment.dem_data.shape, dtype=bool)
            # Mask nan values
            mask_unit[np.isnan(self.catchment.dem_data)] = False

            for criterion_name, criterion in zip(combinations_keys, criteria):
                if criterion_name == 'elevation':
                    mask_elev = np.logical_and(
                        self.catchment.dem_data >= criterion[0],
                        self.catchment.dem_data < criterion[1])
                    mask_unit = np.logical_and(mask_unit, mask_elev)
                elif criterion_name == 'slope':
                    mask_slope = np.logical_and(
                        self.catchment.topography.slope >= criterion[0],
                        self.catchment.topography.slope < criterion[1])
                    mask_unit = np.logical_and(mask_unit, mask_slope)
                elif criterion_name == 'aspect':
                    if criterion == 'N':
                        mask_aspect = np.logical_or(
                            np.logical_and(self.catchment.topography.aspect >= 315,
                                           self.catchment.topography.aspect <= 360),
                            np.logical_and(self.catchment.topography.aspect >= 0,
                                           self.catchment.topography.aspect < 45))
                    elif criterion == 'E':
                        mask_aspect = np.logical_and(
                            self.catchment.topography.aspect >= 45,
                            self.catchment.topography.aspect < 135)
                    elif criterion == 'S':
                        mask_aspect = np.logical_and(
                            self.catchment.topography.aspect >= 135,
                            self.catchment.topography.aspect < 225)
                    elif criterion == 'W':
                        mask_aspect = np.logical_and(
                            self.catchment.topography.aspect >= 225,
                            self.catchment.topography.aspect < 315)
                    else:
                        raise ValueError("Unknown aspect value.")
                    mask_unit = np.logical_and(mask_unit, mask_aspect)
                elif criterion_name == 'radiation':
                    radiation = self.catchment.solar_radiation.mean_annual_radiation
                    mask_radiation = np.logical_and(radiation >= criterion[0],
                                                    radiation < criterion[1])
                    mask_unit = np.logical_and(mask_unit, mask_radiation)

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

            # Set the mean slope of the unit if slope is a criterion
            if 'slope' in criteria_dict.keys():
                i = list(combinations_keys).index('slope')
                slopes = criteria[i]
                res_slope.append(round(float(np.mean(slopes)), 2))
                res_slope_min.append(round(float(slopes[0]), 2))
                res_slope_max.append(round(float(slopes[1]), 2))

            # Get the aspect class if aspect is a criterion
            if 'aspect' in criteria_dict.keys():
                i = list(combinations_keys).index('aspect')
                res_aspect_class.append(criteria[i])

            # Get the radiation class if radiation is a criterion
            if 'radiation' in criteria_dict.keys():
                i = list(combinations_keys).index('radiation')
                radiations = criteria[i]
                res_radiation.append(round(float(np.mean(radiations)), 2))
                res_radiation_min.append(round(float(radiations[0]), 2))
                res_radiation_max.append(round(float(radiations[1]), 2))

            unit_id += 1

        self.catchment.map_unit_ids = self.map_unit_ids.astype(hb.rasterio.uint16)

        if res_elevation:
            self.catchment.hydro_units.add_property(
                ('elevation', 'm'),
                res_elevation
            )
            self.catchment.hydro_units.add_property(
                ('elevation_min', 'm'),
                res_elevation_min
            )
            self.catchment.hydro_units.add_property(
                ('elevation_max', 'm'),
                res_elevation_max
            )

        if res_slope:
            self.catchment.hydro_units.add_property(
                ('slope', 'deg'),
                res_slope
            )
            self.catchment.hydro_units.add_property(
                ('slope_min', 'deg'),
                res_slope_min
            )
            self.catchment.hydro_units.add_property(
                ('slope_max', 'deg'),
                res_slope_max
            )

        if res_aspect_class:
            self.catchment.hydro_units.add_property(
                ('aspect_class', '-'),
                res_aspect_class
            )

        if res_radiation:
            self.catchment.hydro_units.add_property(
                ('radiation', 'W/m2'),
                res_radiation
            )
            self.catchment.hydro_units.add_property(
                ('radiation_min', 'W/m2'),
                res_radiation_min
            )
            self.catchment.hydro_units.add_property(
                ('radiation_max', 'W/m2'),
                res_radiation_max
            )

        self.catchment.initialize_land_cover_fractions()
        self.catchment.get_hydro_units_attributes()
        self.catchment.hydro_units.populate_bounded_instance()
