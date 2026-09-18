from __future__ import annotations

import numpy as np

from hydrobricks._exceptions import DependencyError
from hydrobricks._optional import HAS_XARRAY, xr


class Results:
    """
    Class for the detailed results of a model run.
    This class is used to read the results of a model run (from a netCDF file)
    and to provide methods to extract the results.
    """

    def __init__(self, filename: str) -> None:
        """
        Initialize Results instance from a netCDF file.

        Parameters
        ----------
        filename
            Path to the netCDF results file from a model run.

        Raises
        ------
        DependencyError
            If xarray is not installed.
        FileNotFoundError
            If the specified file does not exist.
        """
        if not HAS_XARRAY:
            raise DependencyError(
                "xarray is required for reading results from netCDF files.",
                package_name="xarray",
                operation="Results.__init__",
                install_command="pip install xarray",
            )
        self.results: xr.Dataset = xr.open_dataset(filename)
        self.labels_distributed: str | list[str] | None = self.results.attrs.get(
            "labels_distributed"
        )
        self.labels_aggregated: str | list[str] | None = self.results.attrs.get(
            "labels_aggregated"
        )
        self.labels_land_cover: list[str] | None = self.results.attrs.get(
            "labels_land_covers"
        )
        self.hydro_units_ids: np.ndarray = self.results.hydro_units_ids.to_numpy()

    def close(self) -> None:
        """Close the netCDF dataset and release the file handle."""
        if hasattr(self, "results") and self.results is not None:
            self.results.close()

    def __enter__(self) -> Results:
        """Context manager entry."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        """Context manager exit — closes the dataset."""
        self.close()
        return False

    def __del__(self) -> None:
        """Fallback cleanup when object is garbage collected."""
        try:
            self.close()
        except Exception:
            pass

    def list_hydro_units_components(self) -> None:
        """
        Print a list of the distributed (hydro unit level) components.

        Displays all component names that have values distributed across individual
        hydro units. These are typically state variables like snowpack, soil moisture,
        or groundwater storage.
        """
        print("Hydro units components:")
        if isinstance(self.labels_distributed, str):
            print("- " + self.labels_distributed)
            return
        elif isinstance(self.labels_distributed, list):
            for label in self.labels_distributed:
                print("- " + label)

    def list_sub_basin_components(self) -> None:
        """
        Print a list of the aggregated (sub-basin level) components.

        Displays all component names that have aggregated values at the sub-basin level.
        These are typically fluxes or flows that are summed across the entire catchment
        (e.g., total runoff, evapotranspiration).
        """
        print("Sub basins components:")
        if isinstance(self.labels_aggregated, str):
            print("- " + self.labels_aggregated)
            return
        elif isinstance(self.labels_aggregated, list):
            for label in self.labels_aggregated:
                print("- " + label)

    def get_land_cover_areas(
        self,
        land_cover: str,
        start_date: str | None = None,
        end_date: str | None = None,
    ) -> np.ndarray:
        """
        Get the areas of a land cover across the hydro units.

        Calculates the spatial distribution of a specific land cover type across
        hydro units over time by multiplying the land cover fractions with the
        hydro unit areas. Supports optional temporal slicing (matching the
        behaviour of get_hydro_units_values).

        Parameters
        ----------
        land_cover
            The name of the land cover type (e.g., 'glacier', 'ground', 'forest').
        start_date
            The start date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns the full time series. Default: None
        end_date
            The end date of the period to extract (format: 'YYYY-MM-DD').
            If None (with start_date set), returns a single-date snapshot. Default: None

        Returns
        -------
        np.ndarray
            Areas of the land cover across the hydro units (2D array: units × time),
            or (units,) for a single date. Units match the hydro unit area units
            (typically m² or km²).

        Raises
        ------
        ValueError
            If the land cover is not found in the results.
        IndexError
            If labels_land_cover is None or empty.
        """
        i_land_cover = self.labels_land_cover.index(land_cover)
        lc_fraction = self.results.land_cover_fractions[i_land_cover, :, :]
        areas = self.results.hydro_units_areas * lc_fraction

        return self._select_time(areas, start_date, end_date)

    def get_hydro_units_structure_ids(self) -> np.ndarray:
        """
        Get the model-structure id used by each hydro unit.

        Units sharing the same subsurface use the same structure; an exclusive land
        cover (e.g. open water) places a unit on a different structure variant. Useful
        to identify which units a given (possibly NaN-omitted) component applies to.

        Returns
        -------
        np.ndarray
            Structure id per hydro unit (1D array, defaults to 1).
        """
        return self.results.hydro_units_structure_ids.to_numpy()

    def get_hydro_units_values(
        self, component: str, start_date: str | None = None, end_date: str | None = None
    ) -> np.ndarray:
        """
        Get the values of a component at the hydro units.

        Retrieves time series or snapshot data for a specific model component
        distributed across hydro units. Supports optional temporal slicing.

        Parameters
        ----------
        component
            The name of the component (e.g., 'snowpack', 'soil_moisture').
            Use list_hydro_units_components() to see available options.
        start_date
            The start date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns full time series from the beginning. Default: None
        end_date
            The end date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns up to end of time series. Default: None

        Returns
        -------
        np.ndarray
            Values of the component at the hydro units.
            Shape: (n_timesteps, n_hydro_units) for time series, or
            (n_hydro_units,) for single date if only start_date provided.

        Raises
        ------
        ValueError
            If the component is not found in the results.
        KeyError
            If date selection fails or dates are not in the time series.
        """
        i_component, _ = self._resolve_component_label(component)

        return self._select_time(
            self.results.hydro_units_values[i_component], start_date, end_date
        )

    def get_mean_hydro_units_values(
        self,
        land_cover: str,
        component: str,
        start_date: str | None = None,
        end_date: str | None = None,
    ) -> np.ndarray:
        """
        Get the mean values of a component across the hydro units weighted
        by land cover area.

        Computes area-weighted average of a component for a specific land cover type,
        accounting for spatial variation in land cover distribution across hydro units.

        Parameters
        ----------
        land_cover
            The name of the land cover type to weight by (e.g., 'glacier',
            'ground', 'forest').
        component
            The name of the component (e.g., 'snowpack', 'soil_moisture').
            Use list_hydro_units_components() to see available options.
        start_date
            The start date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns full time series. Default: None
        end_date
            The end date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns up to end of time series. Default: None

        Returns
        -------
        np.ndarray
            Weighted mean values of the component across the hydro units
            (1D time series).
            Weights are based on the land cover area in each hydro unit.

        Raises
        ------
        ValueError
            If the land cover or component is not found in the results.
        """
        values = self.get_hydro_units_values(component, start_date, end_date)
        lc_areas = self.get_land_cover_areas(land_cover, start_date, end_date)

        return self._area_weighted_nanmean(values, lc_areas, axis=1)

    def get_mean_swe(
        self, start_date: str | None = None, end_date: str | None = None
    ) -> np.ndarray:
        """
        Get the mean snow water equivalent (SWE) across the hydro units weighted
        by land cover.

        Computes the catchment-wide average snow water equivalent by aggregating
        SWE values across all land cover types and hydro units, weighted by their
        respective areas.

        Parameters
        ----------
        start_date
            The start date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns full time series. Default: None
        end_date
            The end date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns up to end of time series. Default: None

        Returns
        -------
        np.ndarray
            Mean SWE across the hydro units (1D time series,
            units: mm water equivalent). A scalar for a single date.

        Notes
        -----
        Land covers without a snowpack (e.g. open water) contribute zero SWE over
        their area, diluting the average as expected.
        """
        lc_swe, lc_areas = self._stack_land_cover_swe(start_date, end_date)

        # Flatten the land-cover and hydro-unit axes together; keep the time axis
        # (if any). Reduces to a scalar for a single-date snapshot.
        lc_swe = lc_swe.reshape(-1, *lc_swe.shape[2:])
        lc_areas = lc_areas.reshape(-1, *lc_areas.shape[2:])

        return self._area_weighted_nanmean(lc_swe, lc_areas, axis=0)

    def get_total_swe(
        self, start_date: str | None = None, end_date: str | None = None
    ) -> np.ndarray:
        """
        Get the total snow water equivalent (SWE) per hydro unit, aggregated
        across land covers.

        SWE is stored per land cover. For each hydro unit this combines the
        per-land-cover snowpack into a single unit-average SWE depth, weighted by
        the land cover areas within the unit. Unlike get_mean_swe(), the hydro unit
        dimension is preserved (no catchment-wide averaging).

        Parameters
        ----------
        start_date
            The start date of the period to extract (format: 'YYYY-MM-DD').
            If None, returns full time series. Default: None
        end_date
            The end date of the period to extract (format: 'YYYY-MM-DD').
            If None (with start_date set), returns a single-date snapshot. Default: None

        Returns
        -------
        np.ndarray
            Total SWE per hydro unit (2D array: units × time,
            units: mm water equivalent), or (units,) for a single date.

        Notes
        -----
        Land covers without a snowpack (e.g. open water) contribute zero SWE over
        their area, diluting the per-unit average as expected.
        """
        lc_swe, lc_areas = self._stack_land_cover_swe(start_date, end_date)

        # Area-weight across land covers (axis 0), keeping the hydro unit dimension.
        return self._area_weighted_nanmean(lc_swe, lc_areas, axis=0)

    def _stack_land_cover_swe(
        self, start_date: str | None, end_date: str | None
    ) -> tuple[np.ndarray, np.ndarray]:
        """Stack per-land-cover snowpack SWE and areas along a leading land-cover axis.

        Returns ``(lc_swe, lc_areas)``, each shaped ``(n_land_covers, n_units, n_time)``
        (or ``(n_land_covers, n_units)`` for a single date), aligned cell-by-cell. A
        land cover without a snowpack component contributes zero SWE over its area, so
        snowless covers (e.g. open water) dilute area-weighted means rather than
        raising.
        """
        lc_swe = []
        lc_areas = []
        for land_cover in self.labels_land_cover:
            areas = np.asarray(
                self.get_land_cover_areas(land_cover, start_date, end_date), dtype=float
            )
            label = f"{land_cover}_snowpack:snow_content"
            if self._has_distributed_component(label):
                swe = np.asarray(
                    self.get_hydro_units_values(label, start_date, end_date),
                    dtype=float,
                )
            else:
                swe = np.zeros_like(areas)
            lc_swe.append(swe)
            lc_areas.append(areas)

        return np.array(lc_swe), np.array(lc_areas)

    def get_time_array(self, start_date: str, end_date: str) -> np.ndarray:
        """
        Get the time array.

        Extracts the time coordinates from the results dataset for a specified
        date range. Useful for creating time-aligned arrays for plotting or analysis.

        Parameters
        ----------
        start_date
            The start date of the period to extract (format: 'YYYY-MM-DD').
        end_date
            The end date of the period to extract (format: 'YYYY-MM-DD').

        Returns
        -------
        np.ndarray
            Array of time values (typically datetime64) for the specified period.

        Raises
        ------
        KeyError
            If dates are not found in the results time coordinates.
        """
        return self.results.time.sel(time=slice(start_date, end_date)).to_numpy()

    @staticmethod
    def _select_time(data, start_date: str | None, end_date: str | None) -> np.ndarray:
        """Slice a time-bearing DataArray and return it as a numpy array.

        No slicing for ``start_date=None``; a single-date snapshot when only
        ``start_date`` is given; a closed ``[start_date, end_date]`` range otherwise.
        """
        if start_date is None:
            return data.to_numpy()
        if end_date is None:
            return data.sel(time=start_date).to_numpy()
        return data.sel(time=slice(start_date, end_date)).to_numpy()

    def _has_distributed_component(self, component: str) -> bool:
        """Whether a distributed component label exists (exact or suffix match)."""
        try:
            self._resolve_component_label(component)
            return True
        except ValueError:
            return False

    @staticmethod
    def _area_weighted_nanmean(
        values: np.ndarray, areas: np.ndarray, axis: int
    ) -> np.ndarray:
        """Area-weighted mean that ignores omitted (NaN) cells.

        With per-hydro-unit structures, a component or land cover absent from a
        unit's structure variant is stored as NaN. Such a cell must not contribute
        to the mean (and must not dilute the weight denominator), so its weight is
        set to zero. NaN in either ``values`` or ``areas`` is treated as absent.
        Reduces to a plain area-weighted mean when there are no NaN cells.
        """
        weights = np.where(np.isnan(values) | np.isnan(areas), 0.0, areas)
        numerator = np.nansum(values * weights, axis=axis)
        denominator = weights.sum(axis=axis)
        with np.errstate(invalid="ignore", divide="ignore"):
            return numerator / denominator

    def _get_distributed_labels(self) -> list[str]:
        """Return distributed labels as a normalized list of strings."""
        if self.labels_distributed is None:
            return []
        if isinstance(self.labels_distributed, str):
            return [self.labels_distributed]
        return [str(label) for label in self.labels_distributed]

    def _resolve_component_label(self, component: str) -> tuple[int, str]:
        """Resolve component name to index, with support for unique suffix matches."""
        labels = self._get_distributed_labels()

        if component in labels:
            return labels.index(component), component

        # Accept shortened names such as "slow_reservoir_2:content" when labels are
        # fully qualified (e.g. "ground_slow_reservoir_2:content").
        suffix_matches = [label for label in labels if label.endswith(component)]
        if len(suffix_matches) == 1:
            resolved = suffix_matches[0]
            return labels.index(resolved), resolved

        if len(suffix_matches) > 1:
            raise ValueError(
                f"Component '{component}' is ambiguous. "
                f"Matching labels: {suffix_matches}."
            )

        available = ", ".join(labels)
        raise ValueError(
            f"Component '{component}' not found in distributed results. "
            f"Available components: {available}."
        )
