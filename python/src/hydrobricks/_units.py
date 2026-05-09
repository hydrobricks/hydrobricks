from enum import auto

import pandas as pd

from hydrobricks._exceptions import DataError
from hydrobricks._optional import StrEnumClass


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


_UNIT_ALIASES: dict[str, Unit] = {
    "mm": Unit.MM,
    "millimeter": Unit.MM,
    "m": Unit.M,
    "meter": Unit.M,
    "km": Unit.KM,
    "kilometer": Unit.KM,
    "m2": Unit.M2,
    "m^2": Unit.M2,
    "m**2": Unit.M2,
    "square meter": Unit.M2,
    "km2": Unit.KM2,
    "km^2": Unit.KM2,
    "km**2": Unit.KM2,
    "square kilometer": Unit.KM2,
    "deg": Unit.DEG,
    "degree": Unit.DEG,
    "degrees": Unit.DEG,
    "year": Unit.YEAR,
    "years": Unit.YEAR,
    "month": Unit.MONTH,
    "months": Unit.MONTH,
    "day": Unit.DAY,
    "days": Unit.DAY,
    "h": Unit.H,
    "hour": Unit.H,
    "hours": Unit.H,
    "s": Unit.S,
    "second": Unit.S,
    "seconds": Unit.S,
    "mm/d": Unit.MM_D,
    "mm/day": Unit.MM_D,
    "mm_d": Unit.MM_D,
    "mm_day": Unit.MM_D,
    "millimeter per day": Unit.MM_D,
    "mm/h": Unit.MM_H,
    "mm/hour": Unit.MM_H,
    "mm_h": Unit.MM_H,
    "mm_hour": Unit.MM_H,
    "millimeter per hour": Unit.MM_H,
    "m/s": Unit.M_S,
    "m/second": Unit.M_S,
    "m_s": Unit.M_S,
    "m_second": Unit.M_S,
    "meter per second": Unit.M_S,
    "°C": Unit.C,
    "C": Unit.C,
    "celsius": Unit.C,
    "°K": Unit.K,
    "K": Unit.K,
    "kelvin": Unit.K,
    "%": Unit.PC,
    "percent": Unit.PC,
    "frac": Unit.FRAC,
    "fraction": Unit.FRAC,
    "MJ/m2/d": Unit.MJ_M2_D,
    "MJ/m2/day": Unit.MJ_M2_D,
    "MJ_m2_d": Unit.MJ_M2_D,
    "MJ_m2_day": Unit.MJ_M2_D,
    "W/m2": Unit.W_M2,
    "W_m2": Unit.W_M2,
    "W/m^2": Unit.W_M2,
    "W/m**2": Unit.W_M2,
    "Watt per square meter": Unit.W_M2,
    "kPa": Unit.KPA,
    "kilopascal": Unit.KPA,
}


def get_unit_enum(unit: str) -> Unit:
    """
    Convert a string representation to a Unit enum value.

    Parameters
    ----------
    unit
        String representation of the unit. Can be in various formats:
        - Enum member name (e.g., 'MM', 'M2')
        - Full names (e.g., 'millimeter', 'square meter')
        - Symbols (e.g., 'mm', 'm', 'm2')
        - Alternative notations (e.g., 'm^2', 'm**2', 'mm/d')

    Returns
    -------
    Unit
        The corresponding Unit enum value.

    Raises
    ------
    ValueError
        If the unit string is not recognized.

    Examples
    --------
    >>> get_unit_enum('mm')
    Unit.MM
    >>> get_unit_enum('square meter')
    Unit.M2
    """
    if unit in Unit.__members__:
        return Unit[unit]

    # Remove brackets, parentheses and spaces
    unit = remove_chars(unit, "([{<)]}> ")

    if unit in ["-", ""] or "Unnamed" in unit:
        return Unit.NO_UNIT

    result = _UNIT_ALIASES.get(unit)
    if result is None:
        raise DataError(
            f"Unknown unit: {unit}", data_type="unit", reason="Unsupported unit type"
        )
    return result


def get_unit_from_df_column(df: pd.DataFrame) -> Unit:
    """
    Get the unit from a DataFrame column header.

    Parameters
    ----------
    df
        A single-column DataFrame with a unit specification in the column name.
        Column name should be a tuple (name, unit) or a string with unit info.

    Returns
    -------
    Unit
        The Unit enum value extracted from the column specification.

    Raises
    ------
    ValueError
        If the DataFrame has more than one column.

    Examples
    --------
    >>> df = pd.DataFrame({('elevation', 'm'): [100, 200, 300]})
    >>> get_unit_from_df_column(df)
    Unit.M
    """
    if len(df.columns) != 1:
        raise DataError(
            "Only single column dataframes are supported.",
            data_type="unit",
            reason="Multiple columns provided",
        )

    return get_unit_enum(df.columns[0])


def convert_unit_df(df: pd.DataFrame, new_unit: Unit | str) -> pd.DataFrame:
    """
    Convert a single-column DataFrame to a new unit.

    The current unit is extracted from the column name (typically a tuple).
    Conversion factors are applied based on source and target units.

    Parameters
    ----------
    df
        Single-column DataFrame with unit specification in column name.
    new_unit
        Target Unit (as Unit enum or string) to convert to.

    Returns
    -------
    pd.DataFrame
        New DataFrame with converted values and updated column name.

    Raises
    ------
    ValueError
        If conversion is not possible or unsupported between the units.

    Examples
    --------
    >>> df = pd.DataFrame({('area', 'km2'): [1.0, 2.0]})
    >>> converted = convert_unit_df(df, Unit.M2)
    """
    unit_from = get_unit_from_df_column(df)
    unit_to = get_unit_enum(new_unit)
    if unit_from == unit_to:
        return df

    return convert_unit(df, unit_from, unit_to)


def convert_unit(
    value: float | pd.DataFrame, unit_from: Unit | str, unit_to: Unit | str
) -> float | pd.DataFrame:
    """
    Convert a numeric value or DataFrame from one unit to another.

    Parameters
    ----------
    value
        The value or DataFrame to convert.
    unit_from
        The source unit (as Unit enum or string).
    unit_to
        The target unit (as Unit enum or string).

    Returns
    -------
    float | pd.DataFrame
        The converted value(s) in the same type as input.

    Raises
    ------
    ValueError
        If the conversion is not implemented for the given unit pair.

    Examples
    --------
    >>> convert_unit(1000.0, Unit.MM, Unit.M)
    1.0
    >>> convert_unit(5.0, 'km', 'm')
    5000.0
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

    raise DataError(
        f"Conversion from {unit_from} to {unit_to} not implemented.",
        data_type="unit",
        reason="Unsupported conversion",
    )


def remove_chars(input_string: str, chars_to_remove: str) -> str:
    """
    Remove specified characters from a string.

    Parameters
    ----------
    input_string
        The string to remove characters from.
    chars_to_remove
        String containing all characters to be removed.

    Returns
    -------
    str
        The string with specified characters removed.

    Examples
    --------
    >>> remove_chars("hello world", " ")
    'helloworld'
    >>> remove_chars("(value)", "()")
    'value'
    """
    result: str = ""

    for char in input_string:
        if char not in chars_to_remove:
            result += char

    return result
