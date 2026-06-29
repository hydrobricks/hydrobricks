"""Observed snow cover fraction as an auxiliary calibration signal.

This module brings observed **snow cover fraction** (e.g. from MODIS) into the
calibration layer as an
:class:`~hydrobricks.evaluation.base.AuxiliaryObservation`:
:class:`SnowCoverObservations` loads the observed per-hydro-unit fractions — either
from a pre-aggregated CSV (:meth:`~SnowCoverObservations.from_csv`) or by aggregating
a remote-sensing raster/NetCDF stack per hydro unit
(:meth:`~SnowCoverObservations.from_netcdf`) — and computes the matching simulated
fractions from a run model (:meth:`~SnowCoverObservations.simulated`), directly from
the in-memory simulation output, so it can be used inside a calibration loop.

From SWE to a snow-cover fraction
---------------------------------
The model stores only the snow water equivalent (SWE) per hydro unit, with no
explicit snow-cover-fraction state. The fraction of a hydro unit that is snow-covered
is therefore derived from the recorded SWE at evaluation time, through a simple linear
depletion curve::

    fraction = min(1, SWE / swe_full)

i.e. the covered fraction grows linearly with SWE until it reaches a full-cover
threshold ``swe_full`` [mm w.e.]. The transform is intentionally kept as a small,
reusable helper (:func:`_swe_to_fraction`) so a future *prognostic* in-model snow
extent could reuse the identical curve.

A hydro unit can carry several land covers (open ground, glacier, ...), each with its
own snowpack. The unit-level cover fraction is the land-cover-fraction-weighted mean of
the per-cover fractions::

    unit_fraction = Σ_lc area_fraction_lc · min(1, SWE_lc / swe_full)

(normalized by the summed land-cover fractions, which span the unit).
"""

from __future__ import annotations

import logging
import warnings
from pathlib import Path
from typing import TYPE_CHECKING, Any

import numpy as np
import pandas as pd

from hydrobricks._exceptions import ConfigurationError, DataError, DependencyError
from hydrobricks._optional import (
    HAS_NETCDF,
    HAS_RASTERIO,
    HAS_RIOXARRAY,
    rxr,
    xr,
)
from hydrobricks.evaluation.base import AuxiliaryObservation, RecordingRequest

if TYPE_CHECKING:
    from hydrobricks.models.model import Model

logger = logging.getLogger(__name__)


def _swe_to_fraction(swe: np.ndarray, swe_full: float) -> np.ndarray:
    """Map SWE [mm w.e.] to a snow-cover fraction via a linear depletion curve.

    ``fraction = min(1, SWE / swe_full)``, clipped to [0, 1]. Non-finite or negative
    SWE values map to 0. Returns an array of the same shape as ``swe``.
    """
    swe = np.asarray(swe, dtype=float)
    with np.errstate(invalid="ignore", divide="ignore"):
        frac = swe / swe_full
    frac = np.clip(frac, 0.0, 1.0)
    return np.nan_to_num(frac, nan=0.0)


class SnowCoverObservations(AuxiliaryObservation):
    """Observed snow cover fraction, used as an auxiliary calibration signal.

    The observations are stored as a flat list of *targets*, each a single
    per-(hydro unit, date) fraction. The matching simulated values are produced by
    :meth:`simulated`, in the same order, so the two can be compared directly (RMSE by
    default).

    Parameters
    ----------
    swe_full
        SWE [mm w.e.] at which a hydro unit is considered fully snow-covered, i.e. the
        threshold of the linear SWE→fraction depletion curve. Default: 100.0.
    land_covers
        Names of the land covers whose snowpacks contribute to the snow cover (e.g.
        ``['ground']`` to ignore glacier snow). Default: ``None`` (all land covers of
        the model).
    metric, weight, mode, tolerance, relative_tolerance
        Calibration configuration, see
        :class:`~hydrobricks.evaluation.base.AuxiliaryObservation`.

    Attributes
    ----------
    targets : list[dict]
        One entry per observed fraction, with keys ``t`` (date, ``pd.Timestamp``),
        ``unit_id`` (int) and ``value`` (fraction in [0, 1]).
    """

    def __init__(
        self,
        swe_full: float = 100.0,
        land_covers: list[str] | None = None,
        metric: str = "rmse",
        weight: float = 1.0,
        mode: str = "objective",
        tolerance: float | None = None,
        relative_tolerance: float | None = None,
    ) -> None:
        if not np.isfinite(swe_full) or swe_full <= 0:
            raise ConfigurationError(
                f"swe_full must be a positive number, got {swe_full}.",
                item_name="swe_full",
                item_value=swe_full,
                reason="Invalid full-cover SWE threshold",
            )
        self.targets: list[dict[str, Any]] = []
        self.swe_full = float(swe_full)
        self.land_covers = list(land_covers) if land_covers is not None else None
        self.metric = metric
        self.weight = weight
        self.mode = mode
        self.tolerance = tolerance
        self.relative_tolerance = relative_tolerance

    # ------------------------------------------------------------------ #
    # Loading
    # ------------------------------------------------------------------ #
    @classmethod
    def from_csv(
        cls,
        path: str | Path,
        *,
        date_col: str | int,
        unit_col: str | int,
        value_col: str | int,
        value_scale: float = 1.0,
        date_format: str | None = None,
        start_date: str | pd.Timestamp | None = None,
        end_date: str | pd.Timestamp | None = None,
        skiprows: int = 0,
        swe_full: float = 100.0,
        land_covers: list[str] | None = None,
        metric: str = "rmse",
        weight: float = 1.0,
        mode: str = "objective",
        tolerance: float | None = None,
        relative_tolerance: float | None = None,
        **read_csv_kwargs: Any,
    ) -> SnowCoverObservations:
        """Load pre-aggregated per-hydro-unit snow cover from a long-format CSV.

        Each row is one observed snow cover fraction for a given hydro unit and date.

        Parameters
        ----------
        path
            Path to the CSV file.
        date_col, unit_col, value_col
            Columns (name or 0-based index) holding the observation date, the hydro
            unit id, and the snow cover value.
        value_scale
            Factor applied to the value column to obtain a fraction in [0, 1] (e.g.
            ``0.01`` for a 0-100 % cover). Default: 1.0.
        date_format
            Optional explicit date format; otherwise dates are inferred.
        start_date, end_date
            Keep only observations whose date lies within this range.
        skiprows
            Rows to skip at the top of the file (metadata header).
        swe_full, land_covers, metric, weight, mode, tolerance, relative_tolerance
            Configuration (see the class docstring).
        **read_csv_kwargs
            Extra keyword arguments forwarded to ``pandas.read_csv``.

        Returns
        -------
        The populated observations object.
        """
        df = pd.read_csv(path, skiprows=skiprows, **read_csv_kwargs)
        dates = pd.to_datetime(
            cls._column(df, date_col), format=date_format, errors="coerce"
        )
        units = pd.to_numeric(cls._column(df, unit_col), errors="coerce")
        values = (
            pd.to_numeric(cls._column(df, value_col), errors="coerce") * value_scale
        )

        obj = cls(
            swe_full=swe_full,
            land_covers=land_covers,
            metric=metric,
            weight=weight,
            mode=mode,
            tolerance=tolerance,
            relative_tolerance=relative_tolerance,
        )
        obj._add_targets(dates.to_numpy(), units.to_numpy(), values.to_numpy())
        if start_date is not None or end_date is not None:
            obj.restrict_to_period(start_date, end_date)
        if not obj.targets:
            logger.warning("No snow cover observations loaded from %s.", path)
        return obj

    @classmethod
    def from_netcdf(
        cls,
        path: str | Path,
        raster_hydro_units: str | Path,
        hydro_units: Any | None = None,
        *,
        var_name: str | None = None,
        file_pattern: str | None = None,
        data_crs: int | None = None,
        dim_time: str = "time",
        dim_x: str = "x",
        dim_y: str = "y",
        value_scale: float = 1.0,
        nodata: float | None = None,
        min_valid_ratio: float = 0.0,
        start_date: str | pd.Timestamp | None = None,
        end_date: str | pd.Timestamp | None = None,
        swe_full: float = 100.0,
        land_covers: list[str] | None = None,
        metric: str = "rmse",
        weight: float = 1.0,
        mode: str = "objective",
        tolerance: float | None = None,
        relative_tolerance: float | None = None,
    ) -> SnowCoverObservations:
        """Load and aggregate a snow-cover raster/NetCDF stack per hydro unit.

        The snow-cover data (e.g. a MODIS time series) is aggregated to the hydro units
        defined by the ``raster_hydro_units`` raster of unit ids: for each date and
        unit, the mean of the valid snow-cover pixels falling in that unit is taken.
        Cloud/nodata pixels are ignored; a unit whose valid-pixel ratio on a date is
        below ``min_valid_ratio`` yields no observation for that date.

        Parameters
        ----------
        path
            Path to a netCDF file, or to a folder of files (with ``file_pattern``).
        raster_hydro_units
            Path to the raster of hydro unit ids used for the spatial aggregation.
        hydro_units
            Optional :class:`~hydrobricks.hydro_units.HydroUnits` (or any object
            supporting ``["id"]``) giving the unit ids to aggregate. If ``None``, the
            ids are taken from the positive unique values of the raster.
        var_name
            Name of the data variable to read. If ``None``, the sole data variable of
            the dataset is used.
        file_pattern
            Glob pattern of the files to read (e.g. ``'*.nc'``). If ``None``, ``path``
            is a single file.
        data_crs
            CRS of the data (EPSG code). If ``None``, read from the file.
        dim_time, dim_x, dim_y
            Names of the time and spatial dimensions (defaults ``'time'``, ``'x'``,
            ``'y'``).
        value_scale
            Factor applied to obtain a fraction in [0, 1] (e.g. ``0.01`` for 0-100 %).
        nodata
            Value flagged as missing (in addition to NaN), e.g. a cloud/fill code.
        min_valid_ratio
            Minimum fraction of a unit's pixels that must be valid on a date for the
            aggregate to be kept (else that date is dropped for the unit). Default: 0.
        start_date, end_date
            Keep only observations whose date lies within this range.
        swe_full, land_covers, metric, weight, mode, tolerance, relative_tolerance
            Configuration (see the class docstring).

        Returns
        -------
        The populated observations object.
        """
        if not HAS_RASTERIO:
            raise DependencyError(
                "rasterio is required to load snow cover from rasters.",
                package_name="rasterio",
                operation="SnowCoverObservations.from_netcdf",
                install_command="pip install rasterio",
            )
        if not HAS_RIOXARRAY:
            raise DependencyError(
                "rioxarray is required to load snow cover from rasters.",
                package_name="rioxarray",
                operation="SnowCoverObservations.from_netcdf",
                install_command="pip install rioxarray",
            )
        if not HAS_NETCDF:
            raise DependencyError(
                "netCDF4 is required to load snow cover from netCDF.",
                package_name="netCDF4",
                operation="SnowCoverObservations.from_netcdf",
                install_command="pip install netCDF4",
            )

        times, unit_ids, matrix = cls._aggregate_per_unit(
            path=path,
            raster_hydro_units=raster_hydro_units,
            hydro_units=hydro_units,
            var_name=var_name,
            file_pattern=file_pattern,
            data_crs=data_crs,
            dim_time=dim_time,
            dim_x=dim_x,
            dim_y=dim_y,
            value_scale=value_scale,
            nodata=nodata,
            min_valid_ratio=min_valid_ratio,
        )

        obj = cls(
            swe_full=swe_full,
            land_covers=land_covers,
            metric=metric,
            weight=weight,
            mode=mode,
            tolerance=tolerance,
            relative_tolerance=relative_tolerance,
        )
        # One target per finite (date, unit) aggregate.
        n_time, n_units = matrix.shape
        for i in range(n_time):
            for j in range(n_units):
                value = matrix[i, j]
                if np.isfinite(value):
                    obj.targets.append(
                        {
                            "t": pd.Timestamp(times[i]),
                            "unit_id": int(unit_ids[j]),
                            "value": float(value),
                        }
                    )
        if start_date is not None or end_date is not None:
            obj.restrict_to_period(start_date, end_date)
        if not obj.targets:
            logger.warning("No snow cover observations loaded from %s.", path)
        return obj

    # ------------------------------------------------------------------ #
    # AuxiliaryObservation interface
    # ------------------------------------------------------------------ #
    def observed(self) -> np.ndarray:
        """The observed snow cover fractions [0, 1], one per target."""
        return np.array([t["value"] for t in self.targets], dtype=float)

    def required_recordings(self, model: Model) -> RecordingRequest:
        """The snowpack series needed by :meth:`simulated`.

        For each contributing land cover this records the snowpack snow content and
        (once) the land-cover fractions, the inputs of the SWE-to-fraction transform.
        A targeted alternative to ``record_all=True``.
        """
        covers = self.land_covers or list(model.land_cover_names)
        request = RecordingRequest(fractions=True)
        for cover in covers:
            request.brick_states.append((f"{cover}_snowpack", "snow_content"))
        return request

    def simulated(self, model: Model) -> np.ndarray:
        """Compute the simulated snow cover fraction matching each observation.

        For each land cover the recorded snowpack SWE is turned into a per-unit cover
        fraction (linear depletion curve, see :func:`_swe_to_fraction`), then combined
        across land covers as a land-cover-fraction-weighted mean. The result is then
        read per target at its (hydro unit, date).

        The snowpack snow content and the land-cover fractions must have been recorded
        in memory — either with ``record_all=True`` or by recording the specific items
        (see :meth:`required_recordings`, applied via ``configure_recording(model)``
        before ``model.setup()``).

        Returns
        -------
        np.ndarray
            Simulated snow cover fraction [0, 1], aligned 1:1 with :meth:`observed`.
            Entries are NaN where the date falls outside the simulation or the unit is
            absent.
        """
        if not self.targets:
            return np.array([], dtype=float)

        covers = self.land_covers or list(model.land_cover_names)
        recorded = set(model.get_recorded_labels())
        time = model.get_recorded_time()  # DatetimeIndex (n_time)
        recorded_ids = model.get_recorded_hydro_unit_ids()  # (n_units,)
        n_units = len(recorded_ids)
        n_time = len(time)

        covered = np.zeros((n_units, n_time), dtype=float)
        total = np.zeros((n_units, n_time), dtype=float)
        found_any = False
        for cover in covers:
            label = f"{cover}_snowpack:snow_content"
            if label not in recorded:
                continue
            found_any = True
            swe = model.get_recorded_hydro_unit_values(label)  # (n_units, n_time)
            frac = model.get_recorded_hydro_unit_fractions(cover)  # (n_units, n_time)
            frac = np.nan_to_num(np.asarray(frac, dtype=float), nan=0.0)
            covered += frac * _swe_to_fraction(swe, self.swe_full)
            total += frac

        if not found_any:
            raise ConfigurationError(
                "No snowpack snow-content series were recorded for the requested land "
                "covers; cannot compute a snow cover fraction. Create the model with "
                "record_all=True or call obs.configure_recording(model) before "
                "model.setup().",
                item_name="land_covers",
                item_value=covers,
                reason="No recorded snowpack series",
            )

        with np.errstate(invalid="ignore", divide="ignore"):
            unit_scf = np.where(total > 0, covered / total, np.nan)

        id_to_row = {int(uid): row for row, uid in enumerate(recorded_ids)}
        sim = np.full(len(self.targets), np.nan, dtype=float)
        for k, target in enumerate(self.targets):
            row = id_to_row.get(int(target["unit_id"]))
            if row is None:
                continue
            ti = _date_index(time, target["t"])
            if ti is None:
                continue
            sim[k] = unit_scf[row, ti]
        return sim

    def restrict_to_period(
        self,
        start: str | pd.Timestamp | None,
        end: str | pd.Timestamp | None,
    ) -> None:
        """Keep only targets whose date lies within ``[start, end]``."""
        start = pd.Timestamp(start) if start is not None else None
        end = pd.Timestamp(end) if end is not None else None
        kept = []
        for t in self.targets:
            if start is not None and t["t"] < start:
                continue
            if end is not None and t["t"] > end:
                continue
            kept.append(t)
        dropped = len(self.targets) - len(kept)
        if dropped:
            logger.info(
                "Dropped %d snow cover observation(s) outside the simulation period.",
                dropped,
            )
        self.targets = kept

    @property
    def values(self) -> np.ndarray:
        """Alias of :meth:`observed` (observed fractions [0, 1])."""
        return self.observed()

    def __len__(self) -> int:
        return len(self.targets)

    # ------------------------------------------------------------------ #
    # Internals
    # ------------------------------------------------------------------ #
    def _add_targets(
        self,
        dates: np.ndarray,
        units: np.ndarray,
        values: np.ndarray,
    ) -> None:
        """Append one target per row, dropping rows with a missing field."""
        for i in range(len(values)):
            t, u, v = dates[i], units[i], values[i]
            if pd.isna(t) or pd.isna(u) or pd.isna(v):
                continue
            self.targets.append(
                {"t": pd.Timestamp(t), "unit_id": int(u), "value": float(v)}
            )

    @staticmethod
    def _column(df: pd.DataFrame, col: str | int) -> pd.Series:
        """Select a column by name or 0-based position."""
        if isinstance(col, int):
            return df.iloc[:, col]
        return df[col]

    @staticmethod
    def _aggregate_per_unit(
        path: str | Path,
        raster_hydro_units: str | Path,
        hydro_units: Any | None,
        var_name: str | None,
        file_pattern: str | None,
        data_crs: int | None,
        dim_time: str,
        dim_x: str,
        dim_y: str,
        value_scale: float,
        nodata: float | None,
        min_valid_ratio: float,
    ) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        """Aggregate a raster/NetCDF stack to a (n_time, n_units) fraction matrix.

        Returns ``(times, unit_ids, matrix)`` where ``times`` is a ``DatetimeIndex``,
        ``unit_ids`` the aggregated unit ids, and ``matrix[i, j]`` the mean snow cover
        of unit ``j`` on date ``i`` (NaN where there is no valid data).
        """
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            unit_da = rxr.open_rasterio(raster_hydro_units).squeeze(drop=True)
        unit_arr = np.asarray(unit_da.values)
        units_nodata = unit_da.rio.nodata

        # Open the data and select the variable as a (time, y, x) DataArray.
        if file_pattern is None:
            ds = xr.open_dataset(path, chunks={})
        else:
            files = sorted(Path(path).glob(file_pattern))
            ds = xr.open_mfdataset(files, chunks={})
        if var_name is not None:
            da = ds[var_name]
        else:
            data_vars = list(ds.data_vars)
            if len(data_vars) != 1:
                raise DataError(
                    "Several variables found in the dataset; specify var_name. "
                    f"Available: {data_vars}.",
                    data_type="snow cover netCDF",
                    reason="Ambiguous data variable",
                )
            da = ds[data_vars[0]]

        da = da.rio.set_spatial_dims(x_dim=dim_x, y_dim=dim_y, inplace=False)
        if data_crs is not None:
            da = da.rio.write_crs(f"epsg:{data_crs}")

        # Align the data to the unit-ids grid only when needed (avoids resampling when
        # the two already share the same CRS and shape).
        same_crs = da.rio.crs is not None and da.rio.crs == unit_da.rio.crs
        same_shape = da.shape[-2:] == unit_arr.shape
        if not (same_crs and same_shape):
            with warnings.catch_warnings():
                warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
                da = da.rio.reproject_match(unit_da)

        y_dim, x_dim = da.rio.y_dim, da.rio.x_dim
        da = da.transpose(dim_time, y_dim, x_dim)
        vals = np.asarray(da.values, dtype=float)  # (n_time, ny, nx)
        times = pd.to_datetime(np.asarray(da[dim_time].values))

        # Unit ids to aggregate.
        if hydro_units is not None:
            ids = np.asarray(hydro_units["id"]).squeeze().astype(int).ravel().tolist()
        else:
            ids = [
                int(u)
                for u in np.unique(unit_arr)
                if np.isfinite(u)
                and u > 0
                and (units_nodata is None or u != units_nodata)
            ]

        n_time = vals.shape[0]
        matrix = np.full((n_time, len(ids)), np.nan, dtype=float)
        for j, uid in enumerate(ids):
            mask = unit_arr == uid
            n_pix = int(mask.sum())
            if n_pix == 0:
                continue
            sub = vals[:, mask]  # (n_time, n_pix_unit)
            valid = np.isfinite(sub)
            if nodata is not None:
                valid &= sub != nodata
            sub = np.where(valid, sub, np.nan)
            count = valid.sum(axis=1)
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", category=RuntimeWarning)
                mean = np.nanmean(sub, axis=1)
            frac_valid = count / n_pix
            keep = (count > 0) & (frac_valid >= min_valid_ratio)
            matrix[:, j] = np.where(keep, mean * value_scale, np.nan)

        return times, np.array(ids, dtype=int), matrix


def _date_index(time: pd.DatetimeIndex, date: pd.Timestamp) -> int | None:
    """Index of ``date`` in the daily time axis, or None if out of range."""
    if date < time[0] or date > time[-1]:
        return None
    pos = int(time.get_indexer([date], method="nearest")[0])
    return pos if pos >= 0 else None
