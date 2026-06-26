"""Observed glacier mass balance as an auxiliary calibration signal.

This module brings observed glacier **mass balance** into the calibration layer as
an :class:`~hydrobricks.evaluation.base.AuxiliaryObservation`:
:class:`GlacierMassBalanceObservations` loads the observed balances from a CSV file
(:meth:`~GlacierMassBalanceObservations.from_csv`, with a GLAMOS preset
:meth:`~GlacierMassBalanceObservations.from_glamos`) and computes the matching
simulated balances from a run model (:meth:`~GlacierMassBalanceObservations.simulated`),
directly from the in-memory simulation output (no netCDF dump), so it can be used
inside a calibration loop.

Definition of the simulated mass balance
-----------------------------------------
We compute the **glaciological surface** mass balance, i.e. accumulation minus
ablation at the glacier surface, which is what the glaciological method (stakes,
as in GLAMOS) measures. Per glacier hydro unit *i* and period ``[t0, t1]``::

    B_i = S_i(t1) - S_i(t0) - Σ_{t0 < t <= t1} M_ice,i(t)

where ``S`` is the glacier snowpack water equivalent (a stock) and ``M_ice`` the
glacier ice-melt flux. This follows from
``B = (P_snow + refreeze) - (M_snow + M_ice)`` and ``dS/dt = P_snow + refreeze -
M_snow``, so the snowfall, snowmelt and refreezing terms collapse into ``ΔS`` and
only the snowpack stock and the ice melt are needed.

This **flux-based surface** balance is preferred over a state difference
``Δ(snow + ice)`` because it excludes ice dynamics. With a delta-h glacier
evolution, the state difference per elevation band is contaminated by the dynamic
mass redistribution (it becomes a *geodetic* per-band balance), whereas GLAMOS
reports the *glaciological* (surface) balance per band. The flux formula stays a
clean surface balance whether or not the geometry evolves, and works with both
the default infinite ice storage and a finite ice storage.

Per-band mm w.e. is normalized by the **model's own (time-varying) glacier area**
per unit, for self-consistency with the simulated geometry. With a finite storage
and an ``ActionGlacierSnowToIceTransformation``, the snow→ice transfer adds a term
to the balance; measure the snowpack stock consistently with the fixed observation
date so the seasonal balance stays exact.
"""

from __future__ import annotations

import logging
import warnings
from pathlib import Path
from typing import TYPE_CHECKING, Any

import numpy as np
import pandas as pd

from hydrobricks._exceptions import ConfigurationError, DataError
from hydrobricks.evaluation.base import AuxiliaryObservation

if TYPE_CHECKING:
    from hydrobricks.models.model import Model

logger = logging.getLogger(__name__)

BALANCE_TYPES = ("annual", "winter", "summer")

# Value-unit conversions to mm water equivalent.
_VALUE_UNIT_TO_MM = {"mm_we": 1.0, "m_we": 1000.0}
# Area-unit conversions to m2.
_AREA_UNIT_TO_M2 = {"m2": 1.0, "km2": 1e6}

_MONTHS = {
    "january": 1,
    "february": 2,
    "march": 3,
    "april": 4,
    "may": 5,
    "june": 6,
    "july": 7,
    "august": 8,
    "september": 9,
    "october": 10,
    "november": 11,
    "december": 12,
}

# Column layout (0-based) of the GLAMOS "fixdate" CSV products. Both the
# whole-glacier and the elevation-bins files share the first eight columns; they
# differ afterwards (the bins file carries the bin area and elevation bounds).
_GLAMOS_COLUMNS = {
    "whole": {
        "glacier_id": 1,
        "date_start": 2,
        "date_winter_end": 3,
        "date_end": 4,
        "Bw": 5,
        "Bs": 6,
        "Ba": 7,
    },
    "elevationbins": {
        "glacier_id": 1,
        "date_start": 2,
        "date_winter_end": 3,
        "date_end": 4,
        "Bw": 5,
        "Bs": 6,
        "Ba": 7,
        "band_area": 8,
        "band_lo": 9,
        "band_hi": 10,
    },
}
_GLAMOS_BALANCE_COLUMN = {"annual": "Ba", "winter": "Bw", "summer": "Bs"}


class GlacierMassBalanceObservations(AuxiliaryObservation):
    """Observed glacier mass balance, used as an auxiliary calibration signal.

    The observations are stored as a flat list of *targets*, each a single scalar
    value with its observation period and (optionally) its elevation band. The
    matching simulated values are produced by :meth:`simulated`, in the same order,
    so the two can be compared directly.

    Parameters
    ----------
    metric, weight, mode, tolerance, relative_tolerance
        Calibration configuration, see
        :class:`~hydrobricks.evaluation.base.AuxiliaryObservation`.

    Attributes
    ----------
    targets : list[dict]
        One entry per scalar observation, with keys ``t0``, ``t1`` (period bounds,
        ``pd.Timestamp``), ``value`` (mm w.e.), ``balance_type``, and ``band_lo`` /
        ``band_hi`` (m a.s.l., or ``None`` for a whole-glacier observation).
    granularity : str
        ``'whole'`` or ``'elevationbins'``.
    """

    def __init__(
        self,
        metric: str = "rmse",
        weight: float = 1.0,
        mode: str = "objective",
        tolerance: float | None = None,
        relative_tolerance: float | None = None,
    ) -> None:
        self.targets: list[dict[str, Any]] = []
        self.granularity: str | None = None
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
        value_col: str | int,
        balance_type: str = "annual",
        *,
        date_start_col: str | int | None = None,
        date_end_col: str | int | None = None,
        year_col: str | int | None = None,
        hydro_year_start: str | int = "October",
        band_lo_col: str | int | None = None,
        band_hi_col: str | int | None = None,
        band_area_col: str | int | None = None,
        value_unit: str = "mm_we",
        area_unit: str = "km2",
        date_format: str | None = None,
        glacier_id_col: str | int | None = None,
        glacier_id: str | None = None,
        start_date: str | pd.Timestamp | None = None,
        end_date: str | pd.Timestamp | None = None,
        skiprows: int = 0,
        metric: str = "rmse",
        weight: float = 1.0,
        mode: str = "objective",
        tolerance: float | None = None,
        relative_tolerance: float | None = None,
        **read_csv_kwargs: Any,
    ) -> GlacierMassBalanceObservations:
        """Load observed glacier mass balance from a generic CSV file.

        Each row is one observed balance for ``balance_type``. The observation
        period is given either by explicit ``date_start_col`` / ``date_end_col``,
        or derived from a ``year_col`` using a ``hydro_year_start`` month (the
        period runs from the 1st of that month of the year to the day before it a
        year later — e.g. October → Oct 1 to Sep 30 of the next year; January →
        the calendar year). Provide ``band_*`` columns to load per-elevation-band
        balances. Call once per balance type for files with several
        (winter/summer/annual) value columns, or use :meth:`from_glamos`.

        Parameters
        ----------
        path
            Path to the CSV file.
        value_col
            Column (name or 0-based index) holding the mass-balance value.
        balance_type
            What ``value_col`` represents: ``'annual'``, ``'winter'`` or
            ``'summer'`` (used only as a label).
        date_start_col, date_end_col
            Columns with the period start/end dates (explicit-period mode).
        year_col, hydro_year_start
            Alternative to explicit dates: a year column and the month the
            hydrological year starts (name or 1-12).
        band_lo_col, band_hi_col, band_area_col
            Columns with the elevation-band bounds [m] and area, for per-band data.
        value_unit, area_unit
            Units of the value (``'mm_we'`` or ``'m_we'``) and band area
            (``'km2'`` or ``'m2'``); normalized to mm w.e. and m2.
        date_format
            Optional explicit date format; otherwise dates are inferred.
        glacier_id_col, glacier_id
            Optional column and value to filter a single glacier from a multi-glacier
            file.
        start_date, end_date
            Keep only observations whose period lies fully within this range.
        skiprows
            Rows to skip at the top of the file (metadata header).
        metric, weight, mode, tolerance, relative_tolerance
            Calibration configuration (see the class docstring).
        **read_csv_kwargs
            Extra keyword arguments forwarded to ``pandas.read_csv``.

        Returns
        -------
        The populated observations object.
        """
        cls._check_balance_type(balance_type)
        if value_unit not in _VALUE_UNIT_TO_MM:
            raise ConfigurationError(
                f"Unknown value_unit '{value_unit}'.",
                item_name="value_unit",
                item_value=value_unit,
                reason=f"Expected {tuple(_VALUE_UNIT_TO_MM)}",
            )
        if area_unit not in _AREA_UNIT_TO_M2:
            raise ConfigurationError(
                f"Unknown area_unit '{area_unit}'.",
                item_name="area_unit",
                item_value=area_unit,
                reason=f"Expected {tuple(_AREA_UNIT_TO_M2)}",
            )

        df = pd.read_csv(path, skiprows=skiprows, **read_csv_kwargs)
        if glacier_id_col is not None and glacier_id is not None:
            col = cls._column(df, glacier_id_col)
            df = df[col.astype(str).str.strip() == glacier_id]
            if df.empty:
                raise DataError(f"No rows found for glacier id '{glacier_id}'.")

        # Period bounds.
        if date_start_col is not None and date_end_col is not None:
            t0 = pd.to_datetime(
                cls._column(df, date_start_col), format=date_format, errors="coerce"
            )
            t1 = pd.to_datetime(
                cls._column(df, date_end_col), format=date_format, errors="coerce"
            )
        elif year_col is not None:
            years = pd.to_numeric(cls._column(df, year_col), errors="coerce")
            t0, t1 = cls._period_from_year(years, hydro_year_start)
        else:
            raise ConfigurationError(
                "Provide either date_start_col + date_end_col, or year_col.",
                item_name="period",
                reason="No period specification",
            )

        values = (
            pd.to_numeric(cls._column(df, value_col), errors="coerce")
            * _VALUE_UNIT_TO_MM[value_unit]
        )
        bands = None
        if band_lo_col is not None and band_hi_col is not None:
            lo = pd.to_numeric(cls._column(df, band_lo_col), errors="coerce")
            hi = pd.to_numeric(cls._column(df, band_hi_col), errors="coerce")
            bands = (lo.to_numpy(), hi.to_numpy())

        obj = cls(
            metric=metric,
            weight=weight,
            mode=mode,
            tolerance=tolerance,
            relative_tolerance=relative_tolerance,
        )
        obj.granularity = "elevationbins" if bands is not None else "whole"
        obj._add_targets(
            t0.to_numpy(), t1.to_numpy(), values.to_numpy(), balance_type, bands
        )
        if start_date is not None or end_date is not None:
            obj.restrict_to_period(start_date, end_date)
        if not obj.targets:
            logger.warning("No glacier mass-balance observations loaded from %s.", path)
        return obj

    @classmethod
    def from_glamos(
        cls,
        path: str | Path,
        kind: str = "whole",
        glacier_id: str | None = None,
        balance_types: tuple[str, ...] | list[str] = ("annual",),
        start_date: str | pd.Timestamp | None = None,
        end_date: str | pd.Timestamp | None = None,
        metric: str = "rmse",
        weight: float = 1.0,
        mode: str = "objective",
        tolerance: float | None = None,
        relative_tolerance: float | None = None,
    ) -> GlacierMassBalanceObservations:
        """Load a GLAMOS "fixdate" mass-balance CSV file (preset over the CSV reader).

        Handles the GLAMOS file layout (metadata/citation header rows, a
        ``date_start`` short-name header row, then a units row).
        The observation periods are taken from the per-row dates, so the hydrological
        year of the data is respected without any extra configuration.

        Parameters
        ----------
        path
            Path to the GLAMOS CSV file.
        kind
            ``'whole'`` for the whole-glacier file (one value per period) or
            ``'elevationbins'`` for the per-elevation-bin file.
        glacier_id
            If given, keep only the rows of this glacier id (e.g. ``'B43-03'``).
        balance_types
            Which balances to use among ``'annual'`` (Ba), ``'winter'`` (Bw) and
            ``'summer'`` (Bs).
        start_date, end_date
            Keep only observations whose period lies fully within this range.
        metric, weight, mode, tolerance, relative_tolerance
            Calibration configuration (see the class docstring).

        Returns
        -------
        The populated observations object.
        """
        if kind not in _GLAMOS_COLUMNS:
            raise ConfigurationError(
                f"Unknown GLAMOS file kind '{kind}'.",
                item_name="kind",
                item_value=kind,
                reason="Expected 'whole' or 'elevationbins'",
            )
        for bt in balance_types:
            cls._check_balance_type(bt)

        cols = _GLAMOS_COLUMNS[kind]
        frame = cls._read_glamos_table(path, cols)
        if glacier_id is not None:
            frame = frame[frame["glacier_id"].astype(str).str.strip() == glacier_id]
            if frame.empty:
                raise DataError(
                    f"No rows found for glacier id '{glacier_id}' in {path}."
                )

        obj = cls(
            metric=metric,
            weight=weight,
            mode=mode,
            tolerance=tolerance,
            relative_tolerance=relative_tolerance,
        )
        obj.granularity = kind
        is_bins = kind == "elevationbins"
        bands = (
            (frame["band_lo"].to_numpy(), frame["band_hi"].to_numpy())
            if is_bins
            else None
        )
        for balance_type in balance_types:
            if balance_type == "annual":
                t0, t1 = frame["date_start"], frame["date_end"]
            elif balance_type == "winter":
                t0, t1 = frame["date_start"], frame["date_winter_end"]
            else:  # summer
                t0, t1 = frame["date_winter_end"], frame["date_end"]
            obj._add_targets(
                t0.to_numpy(),
                t1.to_numpy(),
                frame[_GLAMOS_BALANCE_COLUMN[balance_type]].to_numpy(),
                balance_type,
                bands,
            )

        if start_date is not None or end_date is not None:
            obj.restrict_to_period(start_date, end_date)
        if not obj.targets:
            logger.warning("No glacier mass-balance observations loaded from %s.", path)
        return obj

    # ------------------------------------------------------------------ #
    # AuxiliaryObservation interface
    # ------------------------------------------------------------------ #
    def observed(self) -> np.ndarray:
        """The observed mass-balance values [mm w.e.], one per target."""
        return np.array([t["value"] for t in self.targets], dtype=float)

    def simulated(self, model: Model) -> np.ndarray:
        """Compute the simulated glacier mass balance matching each observation.

        For each target the flux-based surface balance ``B_i = ΔS_i − Σ M_ice,i`` is
        evaluated per glacier hydro unit over the target's period, then aggregated
        (area-weighted by the model's time-varying glacier area) to the target's
        granularity — over all glacierized units for a whole-glacier target, or over
        the units whose elevation lies in the band for an elevation-bin target.

        The model must have been run with ``record_all=True`` (so the glacier
        snowpack, ice melt and land-cover fractions are recorded in memory).

        Returns
        -------
        np.ndarray
            Simulated mass balance [mm w.e.], aligned 1:1 with :meth:`observed`.
            Entries are NaN where the period falls outside the simulation or where
            no glacier area is available.
        """
        if not self.targets:
            return np.array([], dtype=float)

        glacier_covers = [
            name
            for name, cover_type in zip(model.land_cover_names, model.land_cover_types)
            if cover_type == "glacier"
        ]
        if not glacier_covers:
            raise ConfigurationError(
                "The model has no glacier land cover; cannot compute a glacier "
                "mass balance.",
                item_name="land_cover_types",
                reason="No glacier cover",
            )

        time = model.get_recorded_time()  # DatetimeIndex (n_time)

        # Sum the snowpack SWE and ice melt over all glacier covers (n_units, n_time).
        snow = None
        ice_melt = None
        glacier_area = None  # (n_units, n_time), the time-varying glacier area
        areas = model.get_recorded_hydro_unit_areas()  # (n_units,)
        for cover in glacier_covers:
            s = model.get_recorded_hydro_unit_values(f"{cover}_snowpack:snow_content")
            m = model.get_recorded_hydro_unit_values(f"{cover}:melt:output")
            frac = model.get_recorded_hydro_unit_fractions(cover)  # (n_units, n_time)
            area = frac * areas[:, None]
            snow = s if snow is None else np.nansum([snow, s], axis=0)
            ice_melt = m if ice_melt is None else np.nansum([ice_melt, m], axis=0)
            glacier_area = area if glacier_area is None else glacier_area + area

        # Unit elevations, aligned to the recorded (descending-elevation) order.
        recorded_ids = model.get_recorded_hydro_unit_ids()
        elev_by_id = _elevation_by_id(model)
        elevations = np.array([elev_by_id[int(i)] for i in recorded_ids], dtype=float)

        sim = np.full(len(self.targets), np.nan, dtype=float)
        for k, target in enumerate(self.targets):
            i0 = _date_index(time, target["t0"])
            i1 = _date_index(time, target["t1"])
            if i0 is None or i1 is None or i1 <= i0:
                continue

            # Per-unit surface balance over the period (mm w.e.).
            d_snow = snow[:, i1] - snow[:, i0]
            cum_ice_melt = np.nansum(ice_melt[:, i0 + 1 : i1 + 1], axis=1)
            balance_unit = d_snow - cum_ice_melt

            # Representative glacier area per unit over the period (for weighting).
            # Units never glacierized over the period give an all-NaN slice (a
            # harmless empty-slice warning); they are dropped by the mask below.
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", category=RuntimeWarning)
                weight = np.nanmean(glacier_area[:, i0 : i1 + 1], axis=1)

            mask = np.isfinite(balance_unit) & np.isfinite(weight) & (weight > 0)
            if target["band_lo"] is not None:
                mask &= (elevations >= target["band_lo"]) & (
                    elevations < target["band_hi"]
                )
            if not np.any(mask):
                continue

            sim[k] = np.sum(balance_unit[mask] * weight[mask]) / np.sum(weight[mask])

        return sim

    def restrict_to_period(
        self,
        start: str | pd.Timestamp | None,
        end: str | pd.Timestamp | None,
    ) -> None:
        """Keep only targets whose period lies fully within ``[start, end]``."""
        start = pd.Timestamp(start) if start is not None else None
        end = pd.Timestamp(end) if end is not None else None
        kept = []
        for t in self.targets:
            if start is not None and t["t0"] < start:
                continue
            if end is not None and t["t1"] > end:
                continue
            kept.append(t)
        dropped = len(self.targets) - len(kept)
        if dropped:
            logger.info(
                "Dropped %d glacier mass-balance observation(s) outside the "
                "simulation period.",
                dropped,
            )
        self.targets = kept

    @property
    def values(self) -> np.ndarray:
        """Alias of :meth:`observed` (observed values [mm w.e.])."""
        return self.observed()

    def __len__(self) -> int:
        return len(self.targets)

    # ------------------------------------------------------------------ #
    # Internals
    # ------------------------------------------------------------------ #
    def _add_targets(
        self,
        t0: np.ndarray,
        t1: np.ndarray,
        values: np.ndarray,
        balance_type: str,
        bands: tuple[np.ndarray, np.ndarray] | None,
    ) -> None:
        """Append one target per row, dropping rows with a missing value or period."""
        for i in range(len(values)):
            value, a, b = values[i], t0[i], t1[i]
            if pd.isna(value) or pd.isna(a) or pd.isna(b):
                continue
            self.targets.append(
                {
                    "t0": pd.Timestamp(a),
                    "t1": pd.Timestamp(b),
                    "value": float(value),
                    "balance_type": balance_type,
                    "band_lo": float(bands[0][i]) if bands is not None else None,
                    "band_hi": float(bands[1][i]) if bands is not None else None,
                }
            )

    @staticmethod
    def _check_balance_type(balance_type: str) -> None:
        if balance_type not in BALANCE_TYPES:
            raise ConfigurationError(
                f"Unknown balance type '{balance_type}'.",
                item_name="balance_type",
                item_value=balance_type,
                reason=f"Expected one of {BALANCE_TYPES}",
            )

    @staticmethod
    def _column(df: pd.DataFrame, col: str | int) -> pd.Series:
        """Select a column by name or 0-based position."""
        if isinstance(col, int):
            return df.iloc[:, col]
        return df[col]

    @staticmethod
    def _period_from_year(
        years: pd.Series, hydro_year_start: str | int
    ) -> tuple[pd.Series, pd.Series]:
        """Build [start, end] periods from a year column and a start month."""
        if isinstance(hydro_year_start, str):
            month = _MONTHS.get(hydro_year_start.strip().lower())
            if month is None:
                raise ConfigurationError(
                    f"Unknown month '{hydro_year_start}'.",
                    item_name="hydro_year_start",
                    item_value=hydro_year_start,
                    reason="Expected a full English month name or 1-12",
                )
        else:
            month = int(hydro_year_start)
        t0 = pd.to_datetime({"year": years, "month": month, "day": 1}, errors="coerce")
        # End = one year later minus one day (Oct 1 Y -> Sep 30 Y+1; Jan -> Dec 31 Y).
        t1 = t0 + pd.DateOffset(years=1) - pd.Timedelta(days=1)
        return t0, t1

    @staticmethod
    def _read_glamos_table(path: str | Path, cols: dict[str, int]) -> pd.DataFrame:
        """Read a GLAMOS CSV into a normalized DataFrame using column positions.

        The data start is detected automatically (the first row whose
        ``date_start`` column parses as a date), so a varying number of metadata
        header rows is tolerated.
        """
        # GLAMOS files are ragged: metadata/citation rows have few fields while the
        # unquoted "observer" field of the data rows contains extra commas. Force a
        # fixed, wide set of positional columns so the parser does not infer the
        # column count from the first line (the only columns used are 0..10).
        raw = pd.read_csv(
            path,
            header=None,
            dtype=str,
            names=range(40),
            engine="python",
            skip_blank_lines=False,
            encoding="utf-8",
        )
        date_col = cols["date_start"]

        data_start = None
        for i in range(len(raw)):
            parsed = _parse_glamos_dates(pd.Series([raw.iat[i, date_col]]))
            if parsed.notna().iloc[0]:
                data_start = i
                break
        if data_start is None:
            raise DataError(f"Could not find any dated data row in {path}.")

        data = raw.iloc[data_start:].reset_index(drop=True)
        out = pd.DataFrame()
        out["glacier_id"] = data.iloc[:, cols["glacier_id"]]
        out["date_start"] = _parse_glamos_dates(data.iloc[:, cols["date_start"]])
        out["date_winter_end"] = _parse_glamos_dates(
            data.iloc[:, cols["date_winter_end"]]
        )
        out["date_end"] = _parse_glamos_dates(data.iloc[:, cols["date_end"]])
        for key in ("Bw", "Bs", "Ba"):
            out[key] = pd.to_numeric(data.iloc[:, cols[key]], errors="coerce")
        if "band_lo" in cols:
            out["band_lo"] = pd.to_numeric(
                data.iloc[:, cols["band_lo"]], errors="coerce"
            )
            out["band_hi"] = pd.to_numeric(
                data.iloc[:, cols["band_hi"]], errors="coerce"
            )
            out["band_area"] = (
                pd.to_numeric(data.iloc[:, cols["band_area"]], errors="coerce") * 1e6
            )

        return out.dropna(subset=["date_start", "date_end"]).reset_index(drop=True)


def _elevation_by_id(model: Model) -> dict[int, float]:
    """Map hydro unit id -> elevation [m] from the model spatial structure."""
    hu = model.spatial_structure.hydro_units
    ids = hu[("id", "-")].to_numpy()
    elevations = hu[("elevation", "m")].to_numpy()
    return {int(i): float(e) for i, e in zip(ids, elevations)}


def _date_index(time: pd.DatetimeIndex, date: pd.Timestamp) -> int | None:
    """Index of ``date`` in the daily time axis, or None if out of range."""
    if date < time[0] or date > time[-1]:
        return None
    pos = int(time.get_indexer([date], method="nearest")[0])
    return pos if pos >= 0 else None


def _parse_glamos_dates(series: pd.Series) -> pd.Series:
    """Parse a date column that mixes ``yyyy-mm-dd`` and ``dd/mm/yyyy`` formats.

    The two GLAMOS formats are parsed explicitly (rather than with a single
    ``dayfirst`` pass) because ``dayfirst=True`` misreads ISO ``yyyy-mm-dd`` dates.
    Anything not matching either is left to a generic day-first parse.
    """
    text = series.astype(str).str.strip()
    out = pd.to_datetime(text, format="%Y-%m-%d", errors="coerce")
    missing = out.isna()
    if missing.any():
        out[missing] = pd.to_datetime(text[missing], format="%d/%m/%Y", errors="coerce")
    missing = out.isna()
    if missing.any():
        out[missing] = pd.to_datetime(text[missing], dayfirst=True, errors="coerce")
    return out
