"""Soil properties of the hydro units: class tables and soil-map helpers.

:meth:`~hydrobricks.catchment.Catchment.extract_soil_properties` reads three soil
properties per hydro unit from rasters: the available water content [Vol-%], the soil
depth [m] and the hydraulic conductivity. This module helps to get these rasters from
the soil maps at hand:

- National maps often come as class codes rather than as physical values: a map
  tells that a cell belongs to field-capacity class 3, and a table says what that
  class is worth. :data:`SOIL_CLASS_TABLES` holds such tables, named after the map
  they were transcribed from, and :func:`class_table_from_csv` reads one from the
  attribute table of a map (e.g. the HWSD).
- Global products (SoilGrids, HiHydroSoil, EU-SoilHydroGrids) come as values, but
  per depth layer: :func:`integrate_layers` averages the layers over a depth.
- Products giving the soil texture only (SoilGrids) need a pedotransfer function:
  :func:`saxton_rawls` and :func:`hydraulics_from_texture` derive the water content
  and the conductivity from the sand, clay and organic matter contents.
"""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import pandas as pd

from hydrobricks._exceptions import ConfigurationError
from hydrobricks._optional import HAS_RASTERIO, rasterio

#: Available water content [Vol-%] of each class of the Swiss soil suitability map
#: (the ``NFK`` grid), as used by PREVAH (manual part II, p. 19). It feeds the soil
#: moisture capacity through
#: :meth:`~hydrobricks.models.PrevahUniBE.cover_field_capacity`.
SWISS_SOIL_SUITABILITY_AWC_CLASSES: dict[int, float] = {
    1: 1.5,
    2: 4.5,
    3: 8.0,
    4: 11.0,
    5: 16.0,
    6: 24.0,
}

#: Soil depth [m] of each class of the Swiss soil suitability map (the ``BTIF``
#: grid), as used by PREVAH. It caps the rooting depth in the soil moisture capacity.
SWISS_SOIL_SUITABILITY_DEPTH_CLASSES: dict[int, float] = {
    1: 0.1,
    2: 0.25,
    3: 0.5,
    4: 1.0,
    5: 2.0,
    6: 4.0,
}

#: Hydraulic conductivity of each class of the Swiss soil suitability map (the ``K``
#: grid), as used by PREVAH. The values span seven orders of magnitude and are
#: relative (PREVAH states no unit); only their logs relative to the catchment
#: matter, through :meth:`~hydrobricks.models.PrevahUniBE.conductivity_factor`
#: (PREVAH's KWPER).
SWISS_SOIL_SUITABILITY_CONDUCTIVITY_CLASSES: dict[int, float] = {
    1: 0.005,
    2: 0.05,
    3: 0.5,
    4: 5.0,
    5: 50.0,
    6: 500.0,
    7: 5000.0,
}


@dataclass(frozen=True)
class SoilClassTables:
    """
    The class tables of a soil map, one per soil property.

    Attributes
    ----------
    available_water_content
        Class code -> available water content [Vol-%].
    soil_depth
        Class code -> soil depth [m].
    conductivity
        Class code -> hydraulic conductivity, in ``conductivity_unit``.
    conductivity_unit
        Unit of the conductivity values ('-' for relative classes).
    """

    available_water_content: dict[int, float] | None = None
    soil_depth: dict[int, float] | None = None
    conductivity: dict[int, float] | None = None
    conductivity_unit: str = "mm/h"


#: The class tables of the supported soil maps, for the ``reclassify`` argument of
#: :meth:`~hydrobricks.catchment.Catchment.extract_soil_properties`:
#:
#: - ``swiss_soil_suitability``: the Swiss soil suitability map
#:   (Bodeneignungskarte), with PREVAH's values.
SOIL_CLASS_TABLES: dict[str, SoilClassTables] = {
    "swiss_soil_suitability": SoilClassTables(
        available_water_content=SWISS_SOIL_SUITABILITY_AWC_CLASSES,
        soil_depth=SWISS_SOIL_SUITABILITY_DEPTH_CLASSES,
        conductivity=SWISS_SOIL_SUITABILITY_CONDUCTIVITY_CLASSES,
        conductivity_unit="-",
    ),
}


def get_soil_class_tables(name: str) -> SoilClassTables:
    """
    Get the class tables of a supported soil map.

    Parameters
    ----------
    name
        The name of the soil map, a key of :data:`SOIL_CLASS_TABLES`.

    Returns
    -------
    The class tables of the map.
    """
    if name not in SOIL_CLASS_TABLES:
        raise ConfigurationError(
            f"Unknown soil map '{name}': expected one of "
            f"{', '.join(sorted(SOIL_CLASS_TABLES))}.",
            item_name="reclassify",
            item_value=name,
            reason="Unknown soil map",
        )
    return SOIL_CLASS_TABLES[name]


def class_table_from_csv(
    path: str | Path,
    code_column: str,
    value_column: str,
    weight_column: str | None = None,
    scale: float = 1.0,
    **read_csv_kwargs,
) -> dict[int, float]:
    """
    Read a class table from the attribute table of a soil map.

    Soil maps such as the Harmonized World Soil Database (HWSD) give each cell a
    mapping-unit code, the soil properties of each unit being in a separate table.
    This builds the ``reclassify`` mapping from such a table, exported as CSV.

    Parameters
    ----------
    path
        Path of the CSV file.
    code_column
        The column holding the class (mapping-unit) codes of the raster.
    value_column
        The column holding the property value.
    weight_column
        The column weighting the rows sharing a code, for a mapping unit made of
        several soils (e.g. the HWSD ``SHARE`` of each soil in the unit). Default:
        the rows sharing a code are averaged with equal weights.
    scale
        Factor applied to the values (e.g. 100 for a water content in m³/m³ to get
        Vol-%).
    read_csv_kwargs
        Further arguments to :func:`pandas.read_csv` (e.g. ``sep=';'``).

    Returns
    -------
    The mapping of the class codes to the values. The rows without a value are
    ignored.
    """
    table = pd.read_csv(path, **read_csv_kwargs)
    for column in (code_column, value_column, weight_column):
        if column is not None and column not in table.columns:
            raise ConfigurationError(
                f"Column '{column}' not found in {path}.",
                item_name="column",
                item_value=column,
                reason="Missing column",
            )
    table = table.dropna(subset=[code_column, value_column])
    if weight_column is None:
        weights = pd.Series(1.0, index=table.index)
    else:
        weights = table[weight_column].fillna(0.0).astype(float)
    values = table[value_column].astype(float) * scale

    mapping = {}
    for code, rows in table.groupby(code_column).groups.items():
        total = weights[rows].sum()
        if total > 0:
            mapping[int(code)] = float((values[rows] * weights[rows]).sum() / total)

    return mapping


def layer_weights(
    depths: Sequence[tuple[float, float]], max_depth: float | None = None
) -> np.ndarray:
    """
    The weights of soil layers in their mean over a depth.

    Each layer weighs its thickness within ``[0, max_depth]``, so that the mean of
    the layers over that depth is the sum of the weighted layers.

    Parameters
    ----------
    depths
        The top and bottom depth [m] of each layer, e.g. ``(0, 0.05)``,
        ``(0.05, 0.15)``, ... for SoilGrids.
    max_depth
        The depth [m] to average over. Default: the bottom of the deepest layer.

    Returns
    -------
    The weight of each layer (summing to 1).
    """
    depths = np.asarray(depths, dtype=float)
    if depths.ndim != 2 or depths.shape[1] != 2 or np.any(depths[:, 1] <= depths[:, 0]):
        raise ConfigurationError(
            "The layer depths must be (top, bottom) pairs, the bottom below the top.",
            item_name="depths",
            reason="Invalid layer depths",
        )
    if max_depth is None:
        max_depth = float(depths[:, 1].max())
    thickness = np.clip(np.minimum(depths[:, 1], max_depth) - depths[:, 0], 0, None)
    if thickness.sum() <= 0:
        raise ConfigurationError(
            f"No layer lies above the depth {max_depth} m.",
            item_name="max_depth",
            item_value=max_depth,
            reason="Invalid depth",
        )

    return thickness / thickness.sum()


def integrate_layers(
    layers: Sequence[str | Path],
    depths: Sequence[tuple[float, float]],
    output_path: str | Path,
    max_depth: float | None = None,
    scale: float = 1.0,
) -> Path:
    """
    Average the depth layers of a soil property over a depth, into one raster.

    Global soil products give their properties per depth layer. The model reads one
    value per cell: the mean of the layers weighted by their thickness down to
    ``max_depth`` (e.g. the rooting zone). Cells missing in one of the layers are
    left out.

    Parameters
    ----------
    layers
        The paths of the layer rasters, all on the same grid, from the top down.
    depths
        The top and bottom depth [m] of each layer.
    output_path
        The path of the raster to write (GeoTIFF).
    max_depth
        The depth [m] to average over. Default: the bottom of the deepest layer.
    scale
        Factor applied to the values (the products often store scaled integers,
        e.g. 100 to get Vol-% from m³/m³).

    Returns
    -------
    The path of the written raster, to give to
    :meth:`~hydrobricks.catchment.Catchment.extract_soil_properties`.
    """
    _check_rasterio()
    if len(layers) != len(depths):
        raise ConfigurationError(
            f"{len(layers)} layers but {len(depths)} depths are given.",
            item_name="depths",
            reason="Length mismatch",
        )
    weights = layer_weights(depths, max_depth)
    profile = None
    total = None
    for path, weight in zip(layers, weights):
        if weight == 0:
            continue
        data, layer_profile = _read_raster(path)
        if profile is None:
            profile, total = layer_profile, np.zeros(data.shape)
        elif data.shape != total.shape:
            raise ConfigurationError(
                f"The layer {path} is not on the grid of the first layer.",
                item_name="layers",
                reason="Grid mismatch",
            )
        total += weight * data

    return _write_raster(output_path, total * scale, profile)


def saxton_rawls(
    sand: np.ndarray | float,
    clay: np.ndarray | float,
    organic_matter: np.ndarray | float = 2.5,
) -> dict[str, np.ndarray]:
    """
    Soil water characteristics from the soil texture (Saxton & Rawls, 2006).

    The pedotransfer function of Saxton and Rawls (2006), without the density and
    gravel adjustments. It is fitted on soils of up to 8% organic matter and 60%
    clay; outside that range the values are extrapolations.

    Parameters
    ----------
    sand
        Sand content [% by weight].
    clay
        Clay content [% by weight].
    organic_matter
        Organic matter content [% by weight] (about 1.72 times the organic carbon).
        Default: 2.5%.

    Returns
    -------
    The water content at the wilting point (1500 kPa) ``wilting_point``, at the field
    capacity (33 kPa) ``field_capacity`` and at saturation ``saturation`` [Vol-%],
    the available water content ``awc`` (field capacity minus wilting point)
    [Vol-%], and the saturated hydraulic conductivity ``ksat`` [mm/h].
    """
    s = np.asarray(sand, dtype=float) / 100
    c = np.asarray(clay, dtype=float) / 100
    om = np.asarray(organic_matter, dtype=float)

    wp_t = (
        -0.024 * s
        + 0.487 * c
        + 0.006 * om
        + 0.005 * s * om
        - 0.013 * c * om
        + 0.068 * s * c
        + 0.031
    )
    wp = wp_t + (0.14 * wp_t - 0.02)
    fc_t = (
        -0.251 * s
        + 0.195 * c
        + 0.011 * om
        + 0.006 * s * om
        - 0.027 * c * om
        + 0.452 * s * c
        + 0.299
    )
    fc = fc_t + (1.283 * fc_t**2 - 0.374 * fc_t - 0.015)
    sat_fc_t = (
        0.278 * s
        + 0.034 * c
        + 0.022 * om
        - 0.018 * s * om
        - 0.027 * c * om
        - 0.584 * s * c
        + 0.078
    )
    sat_fc = sat_fc_t + (0.636 * sat_fc_t - 0.107)
    sat = fc + sat_fc - 0.097 * s + 0.043

    with np.errstate(divide="ignore", invalid="ignore"):
        b = (np.log(1500) - np.log(33)) / (np.log(fc) - np.log(wp))
        ksat = 1930 * np.clip(sat - fc, 0, None) ** (3 - 1 / b)

    return {
        "wilting_point": wp * 100,
        "field_capacity": fc * 100,
        "saturation": sat * 100,
        "awc": (fc - wp) * 100,
        "ksat": ksat,
    }


def hydraulics_from_texture(
    sand: str | Path,
    clay: str | Path,
    output_dir: str | Path,
    organic_matter: str | Path | None = None,
    scale: dict[str, float] | None = None,
) -> dict[str, Path]:
    """
    Write the available water content and conductivity rasters from texture maps.

    Applies :func:`saxton_rawls` cell by cell, for products giving the soil texture
    only (e.g. SoilGrids, after :func:`integrate_layers`).

    Parameters
    ----------
    sand
        Path of the sand content raster.
    clay
        Path of the clay content raster, on the grid of the sand raster.
    output_dir
        The directory to write ``awc.tif`` [Vol-%] and ``ksat.tif`` [mm/h] into.
    organic_matter
        Path of the organic matter content raster, on the same grid. Default: 2.5%
        everywhere.
    scale
        Factors turning the raster values into % by weight, keyed by 'sand',
        'clay' and 'organic_matter' (e.g. 0.1 for SoilGrids' g/kg). Default: the
        rasters hold %.

    Returns
    -------
    The paths of the written rasters, keyed by 'awc' and 'ksat'.
    """
    _check_rasterio()
    scale = scale or {}
    sand_data, profile = _read_raster(sand)
    clay_data, _ = _read_raster(clay)
    inputs = {"sand": sand_data, "clay": clay_data}
    if organic_matter is not None:
        inputs["organic_matter"], _ = _read_raster(organic_matter)
    for key, data in inputs.items():
        if data.shape != sand_data.shape:
            raise ConfigurationError(
                f"The {key} raster is not on the grid of the sand raster.",
                item_name=key,
                reason="Grid mismatch",
            )
        inputs[key] = data * scale.get(key, 1.0)

    values = saxton_rawls(**inputs)
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    return {
        key: _write_raster(output_dir / f"{key}.tif", values[key], profile)
        for key in ("awc", "ksat")
    }


def _check_rasterio() -> None:
    if not HAS_RASTERIO:
        raise ImportError("rasterio is required to read the soil maps.")


def _read_raster(path: str | Path) -> tuple[np.ndarray, dict]:
    """Read the first band as floats, the nodata cells as NaN."""
    with rasterio.open(path) as src:
        data = src.read(1, masked=True).astype(float).filled(np.nan)
        profile = src.profile

    return data, profile


def _write_raster(path: str | Path, data: np.ndarray, profile: dict) -> Path:
    profile = dict(profile)
    profile.update(dtype="float32", count=1, nodata=np.nan, compress="lzw")
    with rasterio.open(path, "w", **profile) as dst:
        dst.write(data.astype("float32"), 1)

    return Path(path)
