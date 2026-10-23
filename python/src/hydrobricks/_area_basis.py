"""Conversion of recorded hydro-unit values between area bases.

The engine records every hydro-unit value in mm over the area it naturally refers to,
given per label by its *basis*:

- ``"land_cover"``: the content of a land cover or of one of its surface components
  (snowpack, canopy), and the fluxes leaving them unweighted, over the area of that
  land cover;
- ``"hydro_unit"``: the content of the other bricks and the fluxes delivered to a
  brick of the unit, over the hydro unit;
- ``"subbasin"``: the fluxes delivered to the outlet or to a subbasin brick, over the
  local area of the subbasin.

The fluxes are thus recorded as received by their recipient, which keeps the sums
mass-consistent. These helpers express a value over another area instead.
"""

from __future__ import annotations

import numpy as np

from hydrobricks._exceptions import ConfigurationError, DataError

AREA_BASES = ("land_cover", "hydro_unit", "subbasin")


def land_cover_of_label(label: str, land_covers: list[str]) -> str | None:
    """
    Find the land cover a recorded label belongs to, if any.

    A label belongs to a land cover when it is the land cover itself (``<cover>:``)
    or one of its surface components (``<cover>_snowpack:``, ``<cover>_canopy:``).

    Parameters
    ----------
    label
        The recorded component label (e.g. 'glacier_snowpack:snow_content').
    land_covers
        The names of the land covers whose fractions were recorded.

    Returns
    -------
    The land cover name, or None if the label is not tied to a land cover.
    """
    for cover in land_covers:
        if label.startswith((f"{cover}:", f"{cover}_snowpack:", f"{cover}_canopy:")):
            return cover
    return None


def check_area_basis(basis: str) -> None:
    """Raise if the given area basis is not recognized."""
    if basis not in AREA_BASES:
        raise ConfigurationError(
            f"Unknown area basis '{basis}': expected one of {', '.join(AREA_BASES)}.",
            item_name="basis",
            item_value=basis,
            reason="Unknown area basis",
        )


def convert_area_basis(
    values: np.ndarray,
    label: str,
    source: str,
    target: str,
    land_cover_fraction: np.ndarray | None,
    subbasin_fraction: np.ndarray,
) -> np.ndarray:
    """
    Express recorded hydro-unit values over another area.

    Parameters
    ----------
    values
        The recorded values, of shape (n_hydro_units, n_timesteps) or (n_hydro_units,).
    label
        The recorded component label (used in the error messages).
    source
        The area basis the values are recorded over.
    target
        The area basis to express them over.
    land_cover_fraction
        The fraction of the unit covered by the label's land cover, of the same shape
        as ``values``; None when the label is not tied to a land cover.
    subbasin_fraction
        The share of its subbasin's area covered by each hydro unit (n_hydro_units,).

    Returns
    -------
    The values in mm over the target area. Values over a land cover are NaN where
    that land cover is absent from the unit.
    """
    check_area_basis(source)
    check_area_basis(target)
    values = np.asarray(values, dtype=float)
    if source == target:
        return values

    needs_cover = "land_cover" in (source, target)
    if needs_cover and land_cover_fraction is None:
        raise DataError(
            f"The component '{label}' cannot be expressed over its land cover: either "
            f"it is not tied to a land cover, or the land-cover fractions were not "
            f"recorded (enable record_all).",
            data_type="results",
            reason="No land-cover fraction",
        )

    subbasin_fraction = np.asarray(subbasin_fraction, dtype=float)
    if values.ndim == 2:
        subbasin_fraction = subbasin_fraction[:, None]

    # To mm over the hydro unit, then over the target area.
    if source == "land_cover":
        values = values * np.nan_to_num(land_cover_fraction)
    elif source == "subbasin":
        values = values / subbasin_fraction

    if target == "land_cover":
        with np.errstate(invalid="ignore", divide="ignore"):
            values = np.where(
                land_cover_fraction > 0, values / land_cover_fraction, np.nan
            )
    elif target == "subbasin":
        values = values * subbasin_fraction

    return values
