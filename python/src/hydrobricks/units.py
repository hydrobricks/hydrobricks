import sys

if sys.version_info < (3, 11):
    try:
        from strenum import LowercaseStrEnum, StrEnum
    except ImportError:
        raise ImportError("Please install the 'StrEnum' package to use StrEnum "
                          "on Python versions prior to 3.11.")
else:
    from enum import StrEnum

from enum import auto

if sys.version_info < (3, 11):
    StrEnumClass = LowercaseStrEnum
else:
    StrEnumClass = StrEnum


class Unit(StrEnumClass):
    """Enumeration of the units used in HydroBricks."""
    NO_UNIT = auto()  # [-]
    MM = auto()  # [mm]
    M = auto()  # [m]
    KM = auto()  # [km]
    M2 = auto()  # [m2]
    KM2 = auto()  # [km2]
    YEAR = auto()  # [year]
    MONTH = auto()  # [month]
    DAY = auto()  # [day]
    H = auto()  # [h]
    S = auto()  # [s]
    MM_D = auto()  # [mm d-1]
    MM_H = auto()  # [mm h-1]
    M_S = auto()  # [m s-1]
    C = auto()  # [°C]
    K = auto()  # [K]
    PC = auto()  # [%]
    FRAC = auto()  # [fraction]
    MJ_M2_D = auto()  # [MJ m-2 d-1]
    KPA = auto()  # [kPa]


def get_unit_enum(unit):
    """
    Convert a string to a Unit.

    Parameters
    ----------
    unit : str
        The string to convert.

    Returns
    -------
    Unit
        The corresponding Unit.
    """
    if unit in Unit.__members__:
        return Unit[unit]

    if unit in ['-', ''] or 'Unnamed' in unit:
        return Unit.NO_UNIT

    if unit in ['mm', 'millimeter']:
        return Unit.MM
    elif unit in ['m', 'meter']:
        return Unit.M
    elif unit in ['km', 'kilometer']:
        return Unit.KM
    elif unit in ['m2', 'm^2', 'm**2', 'square meter']:
        return Unit.M2
    elif unit in ['km2', 'km^2', 'km**2', 'square kilometer']:
        return Unit.KM2
    elif unit in ['year', 'years']:
        return Unit.YEAR
    elif unit in ['month', 'months']:
        return Unit.MONTH
    elif unit in ['day', 'days']:
        return Unit.DAY
    elif unit in ['h', 'hour', 'hours']:
        return Unit.H
    elif unit in ['s', 'second', 'seconds']:
        return Unit.S
    elif unit in ['mm/d', 'mm/day', 'mm_d', 'mm_day', 'millimeter per day']:
        return Unit.MM_D
    elif unit in ['mm/h', 'mm/hour', 'mm_h', 'mm_hour', 'millimeter per hour']:
        return Unit.MM_H
    elif unit in ['m/s', 'm/second', 'm_s', 'm_second', 'meter per second']:
        return Unit.M_S
    elif unit in ['°C', 'C', 'celsius']:
        return Unit.C
    elif unit in ['°K', 'K', 'kelvin']:
        return Unit.K
    elif unit in ['%', 'percent']:
        return Unit.PC
    elif unit in ['frac', 'fraction']:
        return Unit.FRAC
    elif unit in ['MJ/m2/d', 'MJ/m2/day', 'MJ_m2_d', 'MJ_m2_day']:
        return Unit.MJ_M2_D
    elif unit in ['kPa', 'kilopascal']:
        return Unit.KPA
    else:
        raise ValueError(f"Unknown unit: {unit}")
