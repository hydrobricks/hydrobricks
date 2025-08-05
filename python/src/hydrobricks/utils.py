import datetime
import json
import time
from pathlib import Path

import numpy as np
import pandas as pd
import yaml


def validate_kwargs(kwargs, allowed_kwargs):
    """Checks the keyword arguments against a set of allowed keys."""
    for kwarg in kwargs:
        if kwarg not in allowed_kwargs:
            raise TypeError('Keyword argument not understood:', kwarg)


def dump_config_file(content, directory: str, name: str, file_type: str = 'yaml'):
    directory = Path(directory)

    # Dump YAML file
    if file_type in ['both', 'yaml']:
        with open(directory / f'{name}.yaml', 'w') as outfile:
            yaml.dump(content, outfile, sort_keys=False)

    # Dump JSON file
    if file_type in ['both', 'json']:
        json_object = json.dumps(content, indent=2)
        with open(directory / f'{name}.json', 'w') as outfile:
            outfile.write(json_object)


def date_as_mjd(date: str | pd.Timestamp | pd.DatetimeIndex) -> float | np.ndarray:
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
    From https://gist.github.com/jiffyclub/1294443
    """
    jd = jd + 0.5

    f, i = np.modf(jd)
    i = i.astype(int)

    a = np.trunc((i - 1867216.25) / 36524.25)
    b = np.zeros(len(jd))

    idx = tuple([i > 2299160])
    b[idx] = i[idx] + 1 + a[idx] - np.trunc(a[idx] / 4.)
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
    """Transform a number of days to hours and minutes"""
    hours = days * 24.
    hours, hour = np.modf(hours)

    minutes = hours * 60.
    _, minute = np.modf(minutes)

    return hour.astype(int), minute.astype(int)


def mjd_to_datetime(mjd: np.ndarray) -> np.ndarray:
    """Transform modified julian dates to datetime instances (array-based)."""
    jd = mjd + 2400000.5
    year, month, day = jd_to_date(jd)

    frac_days, day = np.modf(day)
    day = day.astype(int)

    hour, minute = days_to_hours_mins(frac_days)

    date = np.empty(len(mjd), dtype='datetime64[s]')

    for idx, _ in enumerate(year):
        date[idx] = datetime.datetime(
            year[idx], month[idx], day[idx], hour[idx], minute[idx], 0, 0
        )

    return date


def compute_area(shapefile: pd.DataFrame) -> float:
    """Compute the area of a shapefile in square meters."""
    area = 0
    for _, row in shapefile.iterrows():
        poly_area = row.geometry.area
        area += poly_area

    return area


class Timer:
    """Timer to time code execution. Based on: https://pypi.org/project/codetiming/"""

    def __init__(self, text: str | None = None):
        self._start_time = None
        self.last = None
        self.logger = print
        self.text = "Elapsed time: {:0.4f} seconds"
        if text is not None:
            self.text = text

    def start(self):
        """Start a new timer."""
        if self._start_time is not None:
            raise RuntimeError("Timer is running. Use .stop() to stop it")

        self._start_time = time.perf_counter()

    def stop(self, show_time: bool = True) -> float:
        """Stop the timer, and report the elapsed time."""
        if not show_time:
            return 0

        if self._start_time is None:
            raise RuntimeError("Timer is not running. Use .start() to start it")

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
