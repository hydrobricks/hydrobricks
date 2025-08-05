import sys

import pandas as pd

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
    DEG = auto()  # [degree] for lat/lon
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
    W_M2 = auto()  # [W m-2]
    KPA = auto()  # [kPa]


def get_unit_enum(unit: str) -> Unit:
    """
    Convert a string to a Unit.

    Parameters
    ----------
    unit
        The string to convert.

    Returns
    -------
    The corresponding Unit.
    """
    if unit in Unit.__members__:
        return Unit[unit]

    # Remove brackets, parentheses and spaces
    unit = remove_chars(unit, '([{<)]}> ')

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
    elif unit in ['deg', 'degree', 'degrees']:
        return Unit.DEG
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
    elif unit in ['W/m2', 'W_m2', 'W/m^2', 'W/m**2', 'Watt per square meter']:
        return Unit.W_M2
    elif unit in ['kPa', 'kilopascal']:
        return Unit.KPA
    else:
        raise ValueError(f"Unknown unit: {unit}")


def get_unit_from_df_column(df: pd.DataFrame) -> Unit:
    """
    Get the unit of a dataframe column.

    Parameters
    ----------
    df
        The dataframe to get the unit from.

    Returns
    -------
    The unit of the dataframe.
    """
    if len(df.columns) != 1:
        raise ValueError("Only single column dataframes are supported.")

    return get_unit_enum(df.columns[0])


def convert_unit_df(df: pd.DataFrame, new_unit: Unit | str) -> pd.DataFrame:
    """
    Convert a dataframe (single column) to a new unit. The unit of the dataframe
    must be specified in the name of the column (using tuples).

    Parameters
    ----------
    df
        The dataframe to convert.
    new_unit
        The unit to convert to.

    Returns
    -------
    The converted dataframe.
    """
    unit_from = get_unit_from_df_column(df)
    unit_to = get_unit_enum(new_unit)
    if unit_from == unit_to:
        return df

    return convert_unit(df, unit_from, unit_to)


def convert_unit(
        value: float,
        unit_from: Unit | str,
        unit_to: Unit | str
) -> float | pd.DataFrame:
    """
    Convert a value from one unit to another.

    Parameters
    ----------
    value
        The value to convert.
    unit_from
        The unit of the value.
    unit_to
        The unit to convert to.

    Returns
    -------
    The converted value.
    """
    if isinstance(unit_from, str):
        unit_from = get_unit_enum(unit_from)
    if isinstance(unit_to, str):
        unit_to = get_unit_enum(unit_to)

    if unit_from == unit_to:
        return value

    if unit_from == Unit.MM:
        if unit_to == Unit.M:
            return value / 1000
    elif unit_from == Unit.M:
        if unit_to == Unit.MM:
            return value * 1000
        elif unit_to == Unit.KM:
            return value / 1000
    elif unit_from == Unit.KM:
        if unit_to == Unit.M:
            return value * 1000
    elif unit_from == Unit.M2:
        if unit_to == Unit.KM2:
            return value / 1000000
    elif unit_from == Unit.KM2:
        if unit_to == Unit.M2:
            return value * 1000000
    elif unit_from == Unit.DAY:
        if unit_to == Unit.H:
            return value * 24
        elif unit_to == Unit.S:
            return value * 86400
    elif unit_from == Unit.H:
        if unit_to == Unit.DAY:
            return value / 24
        elif unit_to == Unit.S:
            return value * 3600
    elif unit_from == Unit.S:
        if unit_to == Unit.DAY:
            return value / 86400
        elif unit_to == Unit.H:
            return value / 3600
    elif unit_from == Unit.MM_D:
        if unit_to == Unit.MM_H:
            return value / 24
    elif unit_from == Unit.MM_H:
        if unit_to == Unit.MM_D:
            return value * 24

    raise ValueError(f"Conversion from {unit_from} to {unit_to} not implemented.")


def remove_chars(input_string: str, chars_to_remove: str) -> str:
    """
    Remove characters from a string.

    Parameters
    ----------
    input_string
        The string to remove characters from.
    chars_to_remove
        The characters to remove.

    Returns
    -------
    The string with the characters removed.
    """
    result = ""

    for char in input_string:
        if char not in chars_to_remove:
            result += char

    return result
