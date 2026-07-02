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
import re
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
    is_module_available,
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
        valid_min: float | None = None,
        valid_max: float | None = None,
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
        valid_min, valid_max
            Keep only raw values within ``[valid_min, valid_max]`` (applied before
            ``value_scale``); values outside are dropped. Use to filter quality/error
            codes (e.g. ``valid_max=100`` for a 0-100 % product).
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
        raw = pd.to_numeric(cls._column(df, value_col), errors="coerce")
        if valid_min is not None:
            raw = raw.where(raw >= valid_min)
        if valid_max is not None:
            raw = raw.where(raw <= valid_max)
        values = raw * value_scale

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
        valid_min: float | None = None,
        valid_max: float | None = None,
        min_valid_ratio: float = 0.5,
        start_date: str | pd.Timestamp | None = None,
        end_date: str | pd.Timestamp | None = None,
        swe_full: float = 100.0,
        land_covers: list[str] | None = None,
        metric: str = "rmse",
        weight: float = 1.0,
        mode: str = "objective",
        tolerance: float | None = None,
        relative_tolerance: float | None = None,
        engine: str | None = None,
        group: str | None = None,
        cache_dir: str | Path | None = None,
    ) -> SnowCoverObservations:
        """Load and aggregate a snow-cover NetCDF stack per hydro unit.

        See :meth:`_from_stack` for the full parameter description. This is the netCDF
        variant; for HDF5 inputs use :meth:`from_hdf5`.
        """
        return cls._from_stack(
            path,
            raster_hydro_units,
            hydro_units,
            var_name=var_name,
            file_pattern=file_pattern,
            data_crs=data_crs,
            dim_time=dim_time,
            dim_x=dim_x,
            dim_y=dim_y,
            value_scale=value_scale,
            nodata=nodata,
            valid_min=valid_min,
            valid_max=valid_max,
            min_valid_ratio=min_valid_ratio,
            start_date=start_date,
            end_date=end_date,
            swe_full=swe_full,
            land_covers=land_covers,
            metric=metric,
            weight=weight,
            mode=mode,
            tolerance=tolerance,
            relative_tolerance=relative_tolerance,
            engine=engine,
            group=group,
            cache_dir=cache_dir,
            fmt="netcdf",
        )

    @classmethod
    def from_hdf5(
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
        valid_min: float | None = None,
        valid_max: float | None = None,
        min_valid_ratio: float = 0.5,
        start_date: str | pd.Timestamp | None = None,
        end_date: str | pd.Timestamp | None = None,
        swe_full: float = 100.0,
        land_covers: list[str] | None = None,
        metric: str = "rmse",
        weight: float = 1.0,
        mode: str = "objective",
        tolerance: float | None = None,
        relative_tolerance: float | None = None,
        engine: str | None = None,
        group: str | None = None,
        cache_dir: str | Path | None = None,
    ) -> SnowCoverObservations:
        """Load and aggregate a snow-cover HDF5 stack per hydro unit.

        Same as :meth:`from_netcdf` but reads HDF5 files: ``engine`` defaults to
        ``'h5netcdf'`` when available, falling back to ``'netcdf4'`` (which reads
        NetCDF4/HDF5). For data that stores its variable in an HDF5 group, pass
        ``group``. Quality/error codes are filtered with ``valid_min`` / ``valid_max``
        (e.g. ``valid_max=100`` to drop MODIS codes above 100 %).
        See :meth:`_from_stack` for the full parameter description.
        """
        return cls._from_stack(
            path,
            raster_hydro_units,
            hydro_units,
            var_name=var_name,
            file_pattern=file_pattern,
            data_crs=data_crs,
            dim_time=dim_time,
            dim_x=dim_x,
            dim_y=dim_y,
            value_scale=value_scale,
            nodata=nodata,
            valid_min=valid_min,
            valid_max=valid_max,
            min_valid_ratio=min_valid_ratio,
            start_date=start_date,
            end_date=end_date,
            swe_full=swe_full,
            land_covers=land_covers,
            metric=metric,
            weight=weight,
            mode=mode,
            tolerance=tolerance,
            relative_tolerance=relative_tolerance,
            engine=engine,
            group=group,
            cache_dir=cache_dir,
            fmt="hdf5",
        )

    @classmethod
    def _from_stack(
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
        valid_min: float | None = None,
        valid_max: float | None = None,
        min_valid_ratio: float = 0.5,
        start_date: str | pd.Timestamp | None = None,
        end_date: str | pd.Timestamp | None = None,
        swe_full: float = 100.0,
        land_covers: list[str] | None = None,
        metric: str = "rmse",
        weight: float = 1.0,
        mode: str = "objective",
        tolerance: float | None = None,
        relative_tolerance: float | None = None,
        engine: str | None = None,
        group: str | None = None,
        cache_dir: str | Path | None = None,
        fmt: str = "netcdf",
    ) -> SnowCoverObservations:
        """Load and aggregate a snow-cover raster stack per hydro unit.

        The snow-cover data (e.g. a MODIS time series) is aggregated to the hydro units
        defined by the ``raster_hydro_units`` raster of unit ids: for each date and
        unit, the mean of the valid snow-cover pixels falling in that unit is taken.
        Cloud/nodata pixels and out-of-range quality codes are ignored; a unit whose
        valid-pixel ratio on a date is below ``min_valid_ratio`` yields no observation
        for that date.

        Parameters
        ----------
        path
            Path to a data file, or to a folder of files (with ``file_pattern``).
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
            Glob pattern of the files to read (e.g. ``'*.nc'``, ``'*.h5'``). If
            ``None``, ``path`` is a single file.
        data_crs
            CRS of the data (EPSG code). If ``None``, read from the file. When the data
            carries no CRS, it must already be on the same grid as the hydro-unit
            raster (no reprojection is then possible).
        dim_time, dim_x, dim_y
            Names of the time and spatial dimensions (defaults ``'time'``, ``'x'``,
            ``'y'``).
        value_scale
            Factor applied (after filtering) to obtain a fraction in [0, 1] (e.g.
            ``0.01`` for a 0-100 % product).
        nodata
            Value flagged as missing (in addition to NaN), e.g. a fill code.
        valid_min, valid_max
            Keep only raw values within ``[valid_min, valid_max]`` (applied before
            ``value_scale``); values outside are treated as missing. Use this to drop
            quality/error codes, e.g. ``valid_max=100`` for a 0-100 % snow product
            whose codes above 100 flag clouds/water/no-decision.
        min_valid_ratio
            Minimum fraction of a unit's pixels that must be valid on a date for the
            aggregate to be kept (else that date is dropped for the unit). Default: 0.
        start_date, end_date
            Keep only observations whose date lies within this range.
        swe_full, land_covers, metric, weight, mode, tolerance, relative_tolerance
            Configuration (see the class docstring).
        engine
            xarray backend engine. If ``None``, xarray's default is used for
            ``fmt='netcdf'``, and ``'h5netcdf'`` (or ``'netcdf4'``) for ``fmt='hdf5'``.
        group
            Optional HDF5/NetCDF group holding the variable.
        cache_dir
            If given, the aggregated per-(unit, date) fractions are cached there as
            ``snow_cover_<hash>.csv`` (keyed by discretization + options + input files)
            and loaded directly on a later equivalent call. Default: ``None``.
        fmt
            ``'netcdf'`` or ``'hdf5'`` (selects the default engine).

        Returns
        -------
        The populated observations object.
        """
        if not HAS_RASTERIO:
            raise DependencyError(
                "rasterio is required to load snow cover from rasters.",
                package_name="rasterio",
                operation="SnowCoverObservations",
                install_command="pip install rasterio",
            )
        if not HAS_RIOXARRAY:
            raise DependencyError(
                "rioxarray is required to load snow cover from rasters.",
                package_name="rioxarray",
                operation="SnowCoverObservations",
                install_command="pip install rioxarray",
            )
        if engine is None and fmt == "hdf5":
            engine = "h5netcdf" if is_module_available("h5netcdf") else "netcdf4"
        if engine in (None, "netcdf4") and not HAS_NETCDF:
            raise DependencyError(
                "netCDF4 is required to read this data.",
                package_name="netCDF4",
                operation="SnowCoverObservations",
                install_command="pip install netCDF4",
            )
        if engine == "h5netcdf" and not is_module_available("h5netcdf"):
            raise DependencyError(
                "h5netcdf is required to read HDF5 snow cover data.",
                package_name="h5netcdf",
                operation="SnowCoverObservations.from_hdf5",
                install_command="pip install h5netcdf",
            )
        if engine == "h5netcdf" and not is_module_available("h5py"):
            raise DependencyError(
                "h5py is required as the h5netcdf backend to read HDF5 snow "
                "cover data.",
                package_name="h5py",
                operation="SnowCoverObservations.from_hdf5",
                install_command="pip install h5py",
            )

        build_kwargs = dict(
            swe_full=swe_full,
            land_covers=land_covers,
            metric=metric,
            weight=weight,
            mode=mode,
            tolerance=tolerance,
            relative_tolerance=relative_tolerance,
        )
        cache_file, cached = cls._cache_lookup(
            cache_dir,
            raster_hydro_units,
            path,
            file_pattern,
            config={
                "loader": fmt,
                "var_name": var_name,
                "data_crs": data_crs,
                "dim_time": dim_time,
                "dim_x": dim_x,
                "dim_y": dim_y,
                "value_scale": value_scale,
                "nodata": nodata,
                "valid_min": valid_min,
                "valid_max": valid_max,
                "min_valid_ratio": min_valid_ratio,
                "engine": engine,
                "group": group,
                "start_date": str(start_date),
                "end_date": str(end_date),
            },
            build_kwargs=build_kwargs,
        )
        if cached is not None:
            return cached

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
            valid_min=valid_min,
            valid_max=valid_max,
            min_valid_ratio=min_valid_ratio,
            engine=engine,
            group=group,
        )

        obs = cls._build(
            times,
            unit_ids,
            matrix,
            source=path,
            start_date=start_date,
            end_date=end_date,
            **build_kwargs,
        )
        cls._cache_save(obs, cache_file)
        return obs

    @classmethod
    def from_modis(
        cls,
        path: str | Path,
        raster_hydro_units: str | Path,
        hydro_units: Any | None = None,
        *,
        variable: str = "NDSI_Snow_Cover",
        file_pattern: str = "*.hdf",
        date_regex: str = r"A(\d{7})",
        date_format: str = "%Y%j",
        date_parser: Any | None = None,
        value_scale: float = 0.01,
        valid_min: float | None = 0.0,
        valid_max: float | None = 100.0,
        nodata: float | None = None,
        min_valid_ratio: float = 0.5,
        resampling: str = "nearest",
        engine: str = "netcdf4",
        start_date: str | pd.Timestamp | None = None,
        end_date: str | pd.Timestamp | None = None,
        swe_full: float = 100.0,
        land_covers: list[str] | None = None,
        metric: str = "rmse",
        weight: float = 1.0,
        mode: str = "objective",
        tolerance: float | None = None,
        relative_tolerance: float | None = None,
        cache_dir: str | Path | None = None,
    ) -> SnowCoverObservations:
        """Load MODIS (HDF-EOS) daily snow-cover tiles, aggregated per hydro unit.

        Reads HDF-EOS grid products such as MOD10A1 / MYD10A1 (NDSI snow cover). Each
        file holds one date's tile; the date is parsed from the file name, tiles
        sharing a date are mosaicked, and the data are reprojected from the MODIS
        sinusoidal grid (read from the file's ``StructMetadata``) to the hydro-unit
        raster's CRS before aggregating. The default ``valid_min=0`` / ``valid_max=100``
        drop the product's quality/error codes (200=missing, 250=cloud, 255=fill, ...),
        and ``value_scale=0.01`` converts the 0-100 % NDSI snow cover to a fraction.

        Reading the HDF-EOS files uses xarray's ``netcdf4`` engine (the bundled
        ``netCDF4`` reads HDF4-EOS); no separate HDF4/GDAL build is required.

        Parameters
        ----------
        path
            Folder of tiles (with ``file_pattern``) or a single file.
        raster_hydro_units, hydro_units
            The hydro-unit id raster and (optionally) the units to aggregate; see
            :meth:`_from_stack`.
        variable
            Data field to read (default ``'NDSI_Snow_Cover'``).
        file_pattern
            Glob of the tile files (default ``'*.hdf'``).
        date_regex, date_format
            Parse the date from the file name: ``date_regex``'s first group is parsed
            with ``date_format`` (defaults match MODIS ``A%Y%j`` tokens, e.g.
            ``A2025361``).
        date_parser
            Optional callable ``(filename) -> pd.Timestamp`` overriding the regex.
        value_scale, valid_min, valid_max, nodata, min_valid_ratio
            Aggregation/filtering options; see :meth:`_from_stack`.
        resampling
            Resampling for the reprojection to the hydro-unit grid (a
            ``rasterio.enums.Resampling`` name, default ``'nearest'``).
        engine
            xarray engine used to read the files (default ``'netcdf4'``).
        start_date, end_date, swe_full, land_covers, metric, weight, mode, tolerance,
        relative_tolerance
            Configuration; see :meth:`_from_stack` and the class docstring.
        cache_dir
            If given, the aggregated per-(unit, date) fractions are cached there as a
            CSV named ``snow_cover_<hash>.csv``. The hash is built from the hydro-unit
            id raster (the discretization), the aggregation options and a signature of
            the input tiles, so caches never mix across discretizations or settings. On
            a later call with the same inputs the CSV is loaded directly (skipping the
            slow tile reading); otherwise it is written after aggregating. Default:
            ``None`` (no caching).

        Returns
        -------
        The populated observations object.
        """
        if not HAS_RASTERIO:
            raise DependencyError(
                "rasterio is required to load MODIS snow cover.",
                package_name="rasterio",
                operation="SnowCoverObservations.from_modis",
                install_command="pip install rasterio",
            )
        if not HAS_RIOXARRAY:
            raise DependencyError(
                "rioxarray is required to load MODIS snow cover.",
                package_name="rioxarray",
                operation="SnowCoverObservations.from_modis",
                install_command="pip install rioxarray",
            )
        if engine == "netcdf4" and not HAS_NETCDF:
            raise DependencyError(
                "netCDF4 is required to read MODIS HDF-EOS files.",
                package_name="netCDF4",
                operation="SnowCoverObservations.from_modis",
                install_command="pip install netCDF4",
            )

        build_kwargs = dict(
            swe_full=swe_full,
            land_covers=land_covers,
            metric=metric,
            weight=weight,
            mode=mode,
            tolerance=tolerance,
            relative_tolerance=relative_tolerance,
        )
        cache_file, cached = cls._cache_lookup(
            cache_dir,
            raster_hydro_units,
            path,
            file_pattern,
            config={
                "loader": "modis",
                "variable": variable,
                "value_scale": value_scale,
                "valid_min": valid_min,
                "valid_max": valid_max,
                "nodata": nodata,
                "min_valid_ratio": min_valid_ratio,
                "resampling": resampling,
                "start_date": str(start_date),
                "end_date": str(end_date),
            },
            build_kwargs=build_kwargs,
        )
        if cached is not None:
            return cached

        times, unit_ids, matrix = _aggregate_modis(
            path=path,
            raster_hydro_units=raster_hydro_units,
            hydro_units=hydro_units,
            variable=variable,
            file_pattern=file_pattern,
            date_regex=date_regex,
            date_format=date_format,
            date_parser=date_parser,
            value_scale=value_scale,
            nodata=nodata,
            valid_min=valid_min,
            valid_max=valid_max,
            min_valid_ratio=min_valid_ratio,
            resampling=resampling,
            engine=engine,
            start_date=start_date,
            end_date=end_date,
        )

        obs = cls._build(
            times,
            unit_ids,
            matrix,
            source=path,
            start_date=start_date,
            end_date=end_date,
            **build_kwargs,
        )
        cls._cache_save(obs, cache_file)
        return obs

    @classmethod
    def _build(
        cls,
        times: np.ndarray,
        unit_ids: np.ndarray,
        matrix: np.ndarray,
        *,
        source: Any,
        start_date: str | pd.Timestamp | None,
        end_date: str | pd.Timestamp | None,
        swe_full: float,
        land_covers: list[str] | None,
        metric: str,
        weight: float,
        mode: str,
        tolerance: float | None,
        relative_tolerance: float | None,
    ) -> SnowCoverObservations:
        """Build the observation object from a (n_time, n_units) aggregate matrix."""
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
            logger.warning("No snow cover observations loaded from %s.", source)
        return obj

    @classmethod
    def _cache_lookup(
        cls,
        cache_dir: str | Path | None,
        raster_hydro_units: str | Path,
        path: str | Path,
        file_pattern: str | None,
        config: dict,
        build_kwargs: dict,
    ) -> tuple[Path | None, SnowCoverObservations | None]:
        """Resolve the cache file for a request and load it if it already exists.

        Returns ``(cache_file, cached_obs)``: ``cache_file`` is ``None`` when caching is
        off, otherwise the path to use; ``cached_obs`` is the loaded observations on a
        cache hit, else ``None``. The key combines the hydro-unit id raster (the
        discretization), the aggregation ``config`` and a signature of the input files,
        so caches never mix across discretizations or options.
        """
        if cache_dir is None:
            return None, None
        p = Path(path)
        files = sorted(p.glob(file_pattern)) if (file_pattern and p.is_dir()) else [p]
        key = _cache_key(raster_hydro_units, config, _source_signature(files))
        cache_file = Path(cache_dir) / f"snow_cover_{key}.csv"
        if cache_file.exists():
            logger.info("Loading cached snow cover from %s", cache_file)
            return cache_file, cls.from_csv(
                cache_file,
                date_col="date",
                unit_col="unit_id",
                value_col="value",
                **build_kwargs,
            )
        return cache_file, None

    @staticmethod
    def _cache_save(obs: SnowCoverObservations, cache_file: Path | None) -> None:
        """Write the aggregated observations to the cache file (if caching is on)."""
        if cache_file is not None and len(obs) > 0:
            cache_file.parent.mkdir(parents=True, exist_ok=True)
            obs.to_csv(cache_file)
            logger.info("Saved snow cover cache to %s", cache_file)

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

    def to_csv(self, path: str | Path) -> None:
        """Save the per-(hydro unit, date) observations to a long-format CSV.

        The file has ``date``, ``unit_id`` and ``value`` columns and can be read back
        with :meth:`from_csv` (it is also the format used by the raster loaders' cache).
        """
        pd.DataFrame(
            {
                "date": [t["t"] for t in self.targets],
                "unit_id": [t["unit_id"] for t in self.targets],
                "value": [t["value"] for t in self.targets],
            }
        ).to_csv(path, index=False)

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
        valid_min: float | None,
        valid_max: float | None,
        min_valid_ratio: float,
        engine: str | None = None,
        group: str | None = None,
    ) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        """Aggregate a raster stack to a (n_time, n_units) fraction matrix.

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
        open_kwargs: dict[str, Any] = {"chunks": {}}
        if engine is not None:
            open_kwargs["engine"] = engine
        if group is not None:
            open_kwargs["group"] = group
        if file_pattern is None:
            ds = xr.open_dataset(path, **open_kwargs)
        else:
            files = sorted(Path(path).glob(file_pattern))
            if not files:
                raise DataError(
                    f"No files matching '{file_pattern}' found in {path}.",
                    data_type="snow cover stack",
                    reason="No input files",
                )
            ds = xr.open_mfdataset(files, **open_kwargs)
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

        # Align the data to the unit-ids grid. When the data has no CRS (common for raw
        # HDF5), it can only be used if it already shares the unit raster's grid; align
        # otherwise (avoids resampling when CRS and shape already match).
        same_shape = da.shape[-2:] == unit_arr.shape
        if da.rio.crs is None:
            if not same_shape:
                raise DataError(
                    "The snow-cover data has no CRS and its grid does not match the "
                    "hydro-unit raster; pass data_crs, or pre-align the data to the "
                    "unit-ids grid.",
                    data_type="snow cover stack",
                    reason="No CRS and mismatched grid",
                )
            logger.warning(
                "The snow-cover data has no CRS; assuming it shares the hydro-unit "
                "raster grid (no reprojection)."
            )
        elif not (da.rio.crs == unit_da.rio.crs and same_shape):
            with warnings.catch_warnings():
                warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
                da = da.rio.reproject_match(unit_da)

        y_dim, x_dim = da.rio.y_dim, da.rio.x_dim
        da = da.transpose(dim_time, y_dim, x_dim)
        vals = np.asarray(da.values, dtype=float)  # (n_time, ny, nx)
        times = pd.to_datetime(np.asarray(da[dim_time].values))

        return _aggregate_stack(
            vals,
            times,
            unit_arr,
            units_nodata,
            hydro_units,
            nodata,
            valid_min,
            valid_max,
            min_valid_ratio,
            value_scale,
        )


def _date_index(time: pd.DatetimeIndex, date: pd.Timestamp) -> int | None:
    """Index of ``date`` in the daily time axis, or None if out of range."""
    if date < time[0] or date > time[-1]:
        return None
    pos = int(time.get_indexer([date], method="nearest")[0])
    return pos if pos >= 0 else None


def _aggregate_stack(
    vals: np.ndarray,
    times: np.ndarray,
    unit_arr: np.ndarray,
    units_nodata: float | None,
    hydro_units: Any | None,
    nodata: float | None,
    valid_min: float | None,
    valid_max: float | None,
    min_valid_ratio: float,
    value_scale: float,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Aggregate a ``(n_time, ny, nx)`` value stack to a ``(n_time, n_units)`` matrix.

    For each unit and time the mean of the valid pixels (finite, not ``nodata``, and
    within ``[valid_min, valid_max]``) is taken, scaled by ``value_scale``; a time with
    a valid-pixel ratio below ``min_valid_ratio`` is left NaN. ``unit_arr`` must share
    the spatial grid of ``vals``.
    """
    if hydro_units is not None:
        ids = np.asarray(hydro_units["id"]).squeeze().astype(int).ravel().tolist()
    else:
        ids = [
            int(u)
            for u in np.unique(unit_arr)
            if np.isfinite(u) and u > 0 and (units_nodata is None or u != units_nodata)
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
        if valid_min is not None:
            valid &= sub >= valid_min
        if valid_max is not None:
            valid &= sub <= valid_max
        sub = np.where(valid, sub, np.nan)
        count = valid.sum(axis=1)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", category=RuntimeWarning)
            mean = np.nanmean(sub, axis=1)
        frac_valid = count / n_pix
        keep = (count > 0) & (frac_valid >= min_valid_ratio)
        matrix[:, j] = np.where(keep, mean * value_scale, np.nan)

    return times, np.array(ids, dtype=int), matrix


def _parse_struct_metadata(
    meta: str,
) -> tuple[list[float], list[float], int, int, float]:
    """Parse an HDF-EOS ``StructMetadata`` grid block.

    Returns ``(upper_left_xy, lower_right_xy, n_x, n_y, sphere_radius)`` in projection
    metres, as needed to build the grid's affine transform and (sinusoidal) CRS.
    """

    def _tuple(key: str) -> list[float]:
        m = re.search(key + r"=\(([^)]*)\)", meta)
        if m is None:
            raise DataError(
                f"Could not find '{key}' in the HDF-EOS StructMetadata.",
                data_type="MODIS HDF-EOS",
                reason="Missing grid metadata",
            )
        return [float(x) for x in m.group(1).split(",")]

    def _int(key: str) -> int:
        m = re.search(key + r"=(\d+)", meta)
        if m is None:
            raise DataError(
                f"Could not find '{key}' in the HDF-EOS StructMetadata.",
                data_type="MODIS HDF-EOS",
                reason="Missing grid metadata",
            )
        return int(m.group(1))

    ul = _tuple("UpperLeftPointMtrs")
    lr = _tuple("LowerRightMtrs")
    proj_params = _tuple("ProjParams")
    radius = proj_params[0] if proj_params and proj_params[0] > 0 else 6371007.181
    return ul, lr, _int("XDim"), _int("YDim"), radius


def _read_hdf_eos_grid(path: str | Path, variable: str, engine: str) -> Any:
    """Read one HDF-EOS grid field as a georeferenced ``(y, x)`` DataArray.

    The CRS (MODIS sinusoidal) and the affine transform are reconstructed from the
    file's ``StructMetadata``, since the HDF4-EOS georeferencing is not exposed as
    standard coordinates.
    """
    with warnings.catch_warnings():
        # HDF-EOS fields often declare both a _FillValue and a missing_value; xarray
        # warns when decoding them, which is expected here (both map to NaN).
        warnings.filterwarnings("ignore", message=".*multiple fill values.*")
        ds = xr.open_dataset(path, engine=engine)
        da = ds[variable]
    ul, lr, n_x, n_y, radius = _parse_struct_metadata(ds.attrs["StructMetadata.0"])
    px = (lr[0] - ul[0]) / n_x
    py = (ul[1] - lr[1]) / n_y
    xs = ul[0] + (np.arange(n_x) + 0.5) * px
    ys = ul[1] - (np.arange(n_y) + 0.5) * py
    da = da.rename({da.dims[0]: "y", da.dims[1]: "x"})
    da = da.assign_coords(x=("x", xs), y=("y", ys))
    proj4 = (
        f"+proj=sinu +lon_0=0 +x_0=0 +y_0=0 +a={radius} +b={radius} +units=m +no_defs"
    )
    # Avoid extra full-tile copies: only cast when the field is not already floating
    # (xarray decodes fill values to float), and set the CRS in place.
    # The nodata is passed to reproject_match by the caller instead of
    # via write_nodata (which would deep-copy the tile).
    if not np.issubdtype(da.dtype, np.floating):
        da = da.astype(float)
    with warnings.catch_warnings():
        warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
        da.rio.write_crs(proj4, inplace=True)
        da.rio.set_spatial_dims(x_dim="x", y_dim="y", inplace=True)
    return da


def _source_signature(files: list[Path]) -> list[tuple[str, int, int]]:
    """A cheap, order-independent signature of the input files (name, size, mtime).

    Lets the cache detect added, removed or changed tiles without reading them.
    """
    sig = []
    for f in files:
        try:
            st = f.stat()
            sig.append((f.name, int(st.st_size), int(st.st_mtime)))
        except OSError:
            sig.append((f.name, -1, -1))
    return sorted(sig)


def _cache_key(raster_hydro_units: str | Path, config: dict, sources: list) -> str:
    """A hash identifying an aggregation: discretization + options + input files.

    The hydro-unit id raster (which fixes the discretization: geometry *and* the
    per-pixel unit ids) is hashed together with the aggregation options and a
    signature of the source files, so a cache is reused only for a truly equivalent
    request and never mixed across discretizations.
    """
    import hashlib
    import json

    h = hashlib.sha256()
    try:
        h.update(Path(raster_hydro_units).read_bytes())
    except OSError:
        h.update(str(raster_hydro_units).encode())
    h.update(json.dumps(config, sort_keys=True, default=str).encode())
    h.update(json.dumps(sources, sort_keys=True, default=str).encode())
    return h.hexdigest()[:16]


def _date_from_name(
    name: str, date_regex: str, date_format: str, date_parser: Any | None
) -> pd.Timestamp:
    """Parse the observation date from a tile file name."""
    if date_parser is not None:
        return pd.Timestamp(date_parser(name))
    m = re.search(date_regex, name)
    if m is None:
        raise DataError(
            f"Could not parse a date from '{name}' with /{date_regex}/.",
            data_type="MODIS file name",
            reason="Date not found in file name",
        )
    return pd.Timestamp(pd.to_datetime(m.group(1), format=date_format))


def _aggregate_modis(
    path: str | Path,
    raster_hydro_units: str | Path,
    hydro_units: Any | None,
    variable: str,
    file_pattern: str,
    date_regex: str,
    date_format: str,
    date_parser: Any | None,
    value_scale: float,
    nodata: float | None,
    valid_min: float | None,
    valid_max: float | None,
    min_valid_ratio: float,
    resampling: str,
    engine: str,
    start_date: str | pd.Timestamp | None = None,
    end_date: str | pd.Timestamp | None = None,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Read MODIS HDF-EOS tiles, mosaic+reproject per date, and aggregate per unit.

    Each date is reprojected and reduced to a per-unit vector immediately, so only one
    tile is held in memory at a time (a multi-year daily archive would otherwise need
    tens of GB if every grid were stacked). Files outside ``[start_date, end_date]``
    are skipped before reading.
    """
    from rasterio.enums import Resampling
    from rioxarray.merge import merge_arrays

    with warnings.catch_warnings():
        warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
        unit_da = rxr.open_rasterio(raster_hydro_units).squeeze(drop=True)
    unit_arr = np.asarray(unit_da.values)
    units_nodata = unit_da.rio.nodata

    p = Path(path)
    files = sorted(p.glob(file_pattern)) if p.is_dir() else [p]
    if not files:
        raise DataError(
            f"No files matching '{file_pattern}' found in {path}.",
            data_type="MODIS stack",
            reason="No input files",
        )

    # Group tiles by date (a date may be covered by several tiles), skipping dates
    # outside the requested period so a large archive is not read in full.
    lo = pd.Timestamp(start_date) if start_date is not None else None
    hi = pd.Timestamp(end_date) if end_date is not None else None
    by_date: dict[pd.Timestamp, list[Path]] = {}
    for f in files:
        d = _date_from_name(f.name, date_regex, date_format, date_parser)
        if (lo is not None and d < lo) or (hi is not None and d > hi):
            continue
        by_date.setdefault(d, []).append(f)
    times = sorted(by_date)
    if not times:
        return (
            np.array([], dtype="datetime64[ns]"),
            np.array([], dtype=int),
            np.empty((0, 0)),
        )

    # Hydro-unit ids and a per-pixel unit index, computed once.
    if hydro_units is not None:
        ids = np.asarray(hydro_units["id"]).squeeze().astype(int).ravel().tolist()
    else:
        ids = [
            int(u)
            for u in np.unique(unit_arr)
            if np.isfinite(u) and u > 0 and (units_nodata is None or u != units_nodata)
        ]
    n_ids = len(ids)
    flat_units = unit_arr.ravel()
    label = np.full(flat_units.shape, -1, dtype=np.int64)
    for j, uid in enumerate(ids):
        label[flat_units == uid] = j
    in_unit = label >= 0
    n_pix_per_unit = np.bincount(label[in_unit], minlength=n_ids).astype(float)

    resampling_enum = getattr(Resampling, resampling)
    logger.info("Reading %d MODIS date(s) from %s", len(times), path)
    matrix = np.full((len(times), n_ids), np.nan, dtype=float)
    for i, d in enumerate(times):
        tiles = [_read_hdf_eos_grid(f, variable, engine) for f in by_date[d]]
        grid = tiles[0] if len(tiles) == 1 else merge_arrays(tiles, nodata=np.nan)
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=UserWarning)  # pyproj
            grid = grid.rio.reproject_match(
                unit_da, resampling=resampling_enum, nodata=np.nan
            )
        flat = np.asarray(grid.values, dtype=float).ravel()

        valid = np.isfinite(flat)
        if nodata is not None:
            valid &= flat != nodata
        if valid_min is not None:
            valid &= flat >= valid_min
        if valid_max is not None:
            valid &= flat <= valid_max
        valid &= in_unit

        lab = label[valid]
        sums = np.bincount(lab, weights=flat[valid], minlength=n_ids)
        cnts = np.bincount(lab, minlength=n_ids).astype(float)
        safe_pix = np.where(n_pix_per_unit > 0, n_pix_per_unit, 1.0)
        with np.errstate(invalid="ignore", divide="ignore"):
            mean = np.where(cnts > 0, sums / np.where(cnts > 0, cnts, 1.0), np.nan)
            frac_valid = np.where(n_pix_per_unit > 0, cnts / safe_pix, 0.0)
        keep = (cnts > 0) & (frac_valid >= min_valid_ratio)
        matrix[i] = np.where(keep, mean * value_scale, np.nan)

    return np.array(times), np.array(ids, dtype=int), matrix
