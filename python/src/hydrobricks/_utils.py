import datetime
import json
import time
from collections.abc import Callable, Iterable
from pathlib import Path
from typing import Any

import numpy as np
import pandas as pd
import yaml

from hydrobricks._exceptions import ConfigurationError, ModelError


def validate_kwargs(kwargs: dict[str, Any], allowed_kwargs: Iterable[str]) -> None:
    """
    Checks the keyword arguments against a set of allowed keys.

    Parameters
    ----------
    kwargs
        Dictionary of keyword arguments to validate.
    allowed_kwargs
        Iterable of allowed keyword argument names.

    Raises
    ------
    TypeError
        If any keyword argument is not in the allowed set.
    """
    for kwarg in kwargs:
        if kwarg not in allowed_kwargs:
            raise ConfigurationError(
                f"Keyword argument not understood: {kwarg}",
                item_name=kwarg,
                reason="Unknown keyword argument",
            )


def dump_config_file(
    content: dict[str, Any], directory: str | Path, name: str, file_type: str = "yaml"
) -> None:
    """
    Dump configuration content to YAML and/or JSON files.

    Parameters
    ----------
    content
        Dictionary containing configuration data to dump.
    directory
        Path to directory where files will be saved.
    name
        Base name for the output files (without extension).
    file_type
        Type of file(s) to create: 'yaml', 'json', or 'both'. Default: 'yaml'

    Raises
    ------
    ValueError
        If file_type is not one of 'yaml', 'json', or 'both'.
    """
    directory = Path(directory)

    # Dump YAML file
    if file_type in ["both", "yaml"]:
        with open(directory / f"{name}.yaml", "w") as outfile:
            yaml.dump(content, outfile, sort_keys=False)

    # Dump JSON file
    if file_type in ["both", "json"]:
        json_object = json.dumps(content, indent=2)
        with open(directory / f"{name}.json", "w") as outfile:
            outfile.write(json_object)


def date_as_mjd(date: str | pd.Timestamp | pd.DatetimeIndex) -> float | np.ndarray:
    """
    Convert date(s) to modified Julian date (MJD).

    Converts a date or array of dates to modified Julian day number (MJD).
    MJD = JD - 2400000.5, where JD is the Julian date.

    Parameters
    ----------
    date
        Date to convert. Can be a string (parsed as ISO format), pandas Timestamp,
        or DatetimeIndex.

    Returns
    -------
    float | np.ndarray
        Modified Julian date(s). Returns float for single date,
        ndarray for multiple dates.

    Notes
    -----
    Modified Julian Day (MJD) starts at midnight on November 17, 1858.
    """
    if isinstance(date, str):
        return pd.to_datetime(date).to_julian_date() - 2400000.5
    if isinstance(date, pd.Timestamp):
        return date.to_julian_date() - 2400000.5
    mjd = pd.DatetimeIndex(date).to_julian_date() - 2400000.5
    mjd = mjd.values
    return mjd


def jd_to_date(jd: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """
    Transform julian date numbers to year, month and day (array-based).

    Converts an array of Julian day numbers to calendar date components.
    Uses the algorithm from https://gist.github.com/jiffyclub/1294443

    Parameters
    ----------
    jd
        Array of Julian day numbers to convert.

    Returns
    -------
    tuple[np.ndarray, np.ndarray, np.ndarray]
        Tuple of (year, month, day) arrays as integers.

    Notes
    -----
    This algorithm handles both proleptic Gregorian (JD > 2299160) and
    Julian calendar dates (JD <= 2299160).

    References
    ----------
    https://gist.github.com/jiffyclub/1294443
    """
    jd = jd + 0.5

    f, i = np.modf(jd)
    i = i.astype(int)

    a = np.trunc((i - 1867216.25) / 36524.25)
    b = np.zeros(len(jd))

    idx = tuple([i > 2299160])
    b[idx] = i[idx] + 1 + a[idx] - np.trunc(a[idx] / 4.0)
    idx = tuple([i <= 2299160])
    b[idx] = i[idx]

    c = b + 1524
    d = np.trunc((c - 122.1) / 365.25)
    e = np.trunc(365.25 * d)
    g = np.trunc((c - e) / 30.6001)

    day = c - e + f - np.trunc(30.6001 * g)

    month = np.zeros(len(jd))
    month[g < 13.5] = g[g < 13.5] - 1
    month[g >= 13.5] = g[g >= 13.5] - 13
    month = month.astype(int)

    year = np.zeros(len(jd))
    year[month > 2.5] = d[month > 2.5] - 4716
    year[month <= 2.5] = d[month <= 2.5] - 4715
    year = year.astype(int)

    return year, month, day


def days_to_hours_mins(days: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """
    Transform a number of days to hours and minutes.

    Converts fractional days into hours and minutes components. Useful for
    converting time differences to human-readable format.

    Parameters
    ----------
    days
        Array of fractional days to convert.

    Returns
    -------
    tuple[np.ndarray, np.ndarray]
        Tuple of (hours, minutes) as integer arrays.

    Examples
    --------
    >>> hours, minutes = days_to_hours_mins(np.array([0.5, 1.25]))
    >>> hours
    array([12, 30])
    >>> minutes
    array([0, 0])
    """
    hours = days * 24.0
    hours, hour = np.modf(hours)

    minutes = hours * 60.0
    _, minute = np.modf(minutes)

    return hour.astype(int), minute.astype(int)


def mjd_to_datetime(mjd: np.ndarray) -> np.ndarray:
    """
    Transform modified julian dates to datetime instances (array-based).

    Converts an array of modified Julian day numbers to numpy datetime64 objects.
    This is the inverse operation of date_as_mjd() for arrays.

    Parameters
    ----------
    mjd
        Array of modified Julian day numbers to convert.

    Returns
    -------
    np.ndarray
        Array of datetime64[s] (datetime in seconds precision) objects.

    Notes
    -----
    Modified Julian Day (MJD) starts at midnight on November 17, 1858.
    Uses the conversion: JD = MJD + 2400000.5

    Examples
    --------
    >>> mjd = np.array([50000, 50001])
    >>> dates = mjd_to_datetime(mjd)
    >>> dates
    array(['1995-10-10', '1995-10-11'], dtype='datetime64[s]')
    """
    jd = mjd + 2400000.5
    year, month, day = jd_to_date(jd)

    frac_days, day = np.modf(day)
    day = day.astype(int)

    hour, minute = days_to_hours_mins(frac_days)

    date = np.empty(len(mjd), dtype="datetime64[s]")

    for idx, _ in enumerate(year):
        date[idx] = datetime.datetime(
            year[idx], month[idx], day[idx], hour[idx], minute[idx], 0, 0
        )

    return date


def compute_area(shapefile: pd.DataFrame) -> float:
    """
    Compute the total area of geometries in a shapefile-like DataFrame.

    Parameters
    ----------
    shapefile
        DataFrame with a 'geometry' column containing shapely geometry objects
        (e.g., from geopandas.GeoDataFrame).

    Returns
    -------
    float
        Total area in the same units as the geometry objects (typically square meters).

    Examples
    --------
    >>> import geopandas as gpd
    >>> gdf = gpd.read_file('shapes.shp')
    >>> total_area = compute_area(gdf)
    """
    area: float = 0.0
    for _, row in shapefile.iterrows():
        poly_area = row.geometry.area
        area += poly_area

    return area


class Timer:
    """Timer to time code execution. Based on: https://pypi.org/project/codetiming/"""

    def __init__(self, text: str | Callable[[float], str] | None = None) -> None:
        """
        Initialize the timer.

        Parameters
        ----------
        text
            Format string or callable for elapsed time output. If a string, can include
            format placeholders: {milliseconds}, {seconds}, {minutes}. If callable,
            receives elapsed time in seconds and returns a formatted string.
            Default: "Elapsed time: {seconds:.4f} seconds"
        """
        self._start_time: float | None = None
        self.last: float | None = None
        self.logger: Callable[[str], None] = print
        self.text: str | Callable[[float], str] = "Elapsed time: {seconds:.4f} seconds"
        if text is not None:
            self.text = text

    def start(self) -> None:
        """
        Start a new timer.

        Raises
        ------
        RuntimeError
            If the timer is already running.
        """
        if self._start_time is not None:
            raise ModelError("Timer is running. Use .stop() to stop it")

        self._start_time = time.perf_counter()

    def stop(self, show_time: bool = True) -> float:
        """
        Stop the timer and report the elapsed time.

        Parameters
        ----------
        show_time
            If True, print the elapsed time using the logger. Default: True

        Returns
        -------
        float
            Elapsed time in seconds. Returns 0.0 if show_time is False.

        Raises
        ------
        RuntimeError
            If the timer is not running.
        """
        if not show_time:
            return 0.0

        if self._start_time is None:
            raise ModelError("Timer is not running. Use .start() to start it")

        # Calculate elapsed time
        self.last = time.perf_counter() - self._start_time
        self._start_time = None

        # Report elapsed time
        if self.logger:
            if callable(self.text):
                text = self.text(self.last)
            else:
                attributes = {
                    "milliseconds": self.last * 1000,
                    "seconds": self.last,
                    "minutes": self.last / 60,
                }
                text = self.text.format(self.last, **attributes)
            self.logger(text)

        return self.last
