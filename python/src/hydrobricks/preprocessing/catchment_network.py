"""Delineation of the subbasins of a river network from outlet points or polygons."""

from __future__ import annotations

import heapq
import logging
import math
import warnings
from pathlib import Path
from typing import TYPE_CHECKING, Any

import numpy as np
import pandas as pd

from hydrobricks._exceptions import ConfigurationError, DataError, DependencyError
from hydrobricks._optional import (
    HAS_GEOPANDAS,
    HAS_PYSHEDS,
    HAS_RASTERIO,
    gpd,
    pyshedsGrid,
    rasterio,
)

warnings.filterwarnings("ignore", category=DeprecationWarning, module="pysheds")

if TYPE_CHECKING:
    from hydrobricks.catchment import Catchment

logger = logging.getLogger(__name__)

# The pysheds D8 direction codes and the (row, column) offsets they point to.
_D8_OFFSETS: dict[int, tuple[int, int]] = {
    64: (-1, 0),  # N
    128: (-1, 1),  # NE
    1: (0, 1),  # E
    2: (1, 1),  # SE
    4: (1, 0),  # S
    8: (1, -1),  # SW
    16: (0, -1),  # W
    32: (-1, -1),  # NW
}

#: Minimum slope kept for a reach [m/m] (flat reaches from a coarse DEM).
MIN_SLOPE = 1e-4

#: Tolerance [m] within which two river line ends are the same node of the network.
NODE_TOLERANCE = 1.0

#: Bounds on the river network length of a reach relative to its flow path length.
#: Outside them the path found on the network is deemed wrong and the flow path is kept.
LENGTH_RATIO_BOUNDS = (0.5, 3.0)

#: Search radius [cells] around the outlet of a subbasin polygon for the subbasin it
#: drains into, when the topology is found on the DEM.
OUTLET_SEARCH_RADIUS = 10


class CatchmentNetwork:
    """
    Delineate the subbasins of a river network from outlet points (gauges), or
    read them from polygons.

    Each outlet is snapped to the stream network derived from the DEM flow
    accumulation (or to a given river network), its upstream area is delineated
    with the D8 flow directions (``pysheds``), and the nested areas are carved out
    so that every subbasin holds only its own (local) area. Subbasin polygons, local
    or nested, take the place of that delineation. The hydro units are created
    afterwards, within the subbasins (no hydro unit may span two of them), and the
    reach of each subbasin (the main channel from the entry of its upstream
    subbasins to its outlet) is measured: length along the D8 flow path or the river
    network, and slope from the elevations at its ends.

    The result is the subbasin table consumed by
    :meth:`HydroUnits.set_subbasins <hydrobricks.HydroUnits.set_subbasins>`
    (columns ``id``, ``downstream``, ``name``, ``length``, ``slope``, ...), set on
    the catchment's hydro units, plus the ``subbasin`` column of the hydro units
    and a raster of subbasin ids (:attr:`map_subbasin_ids`).
    """

    def __init__(self, catchment: Catchment) -> None:
        """
        Initialize the network delineation for a catchment.

        Parameters
        ----------
        catchment
            The catchment object (with a DEM and hydro units).
        """
        self.catchment: Catchment = catchment
        #: The subbasin id of every DEM cell (0 outside the catchment).
        self.map_subbasin_ids: np.ndarray | None = None
        #: The subbasin table of the last delineation.
        self.table: pd.DataFrame | None = None
        self._grid: Any = None
        self._fdir: Any = None
        self._acc: np.ndarray | None = None
        self._elevation: np.ndarray | None = None

    # --- Public API -------------------------------------------------------------------

    def delineate(
        self,
        outlets: Any = None,
        names: list[str] | None = None,
        snap_distance: float | None = None,
        stream_threshold_area: float = 1e6,
        river_network: str | Path | Any | None = None,
        reach_lengths: str = "auto",
        outlet_name: str = "outlet",
        subbasins: str | Path | Any | None = None,
        id_column: str | None = None,
        downstream_column: str | None = None,
        name_column: str = "name",
    ) -> pd.DataFrame:
        """
        Delineate the subbasins draining to the given outlets, or read them from
        polygons.

        The subbasins come either from outlet points (``outlets``), whose upstream
        areas are delineated on the DEM flow directions, or from subbasin polygons
        (``subbasins``). The polygons can be the local areas of the subbasins (a
        partition of the catchment), the areas drained at each gauge (nested in each
        other), or a mix of both: every DEM cell goes to the smallest polygon covering
        it, which carves the nested areas out of the larger ones. Unless
        ``downstream_column`` is given, the subbasin a polygon drains into is found on
        the DEM, from the cell just downstream of its outlet (its lowest cell, or the
        cell of largest flow accumulation when the flow directions are computed).

        With polygons and a ``river_network``, the flow directions are not computed
        at all: the reach lengths are measured along the river lines (the reach of a
        headwater subbasin up to the farthest point of the network within it), and
        ``pysheds`` is not needed.

        The subbasins come before the hydro units, as no hydro unit may span two
        subbasins: the hydro units created afterwards by :meth:`discretize_by
        <hydrobricks.preprocessing.CatchmentDiscretization.discretize_by>` each lie
        within a single subbasin, and a unit raster loaded afterwards is checked
        against them. Delineating the subbasins of a catchment that already has hydro
        units is refused.

        Parameters
        ----------
        outlets
            The outlet points (gauges), in the catchment CRS unless a vector file or
            GeoDataFrame carries its own CRS: a path to a point vector file, a
            GeoDataFrame of points, a list of ``(x, y)`` pairs, or a mapping of
            name to ``(x, y)``. A ``name`` column of a vector file names the
            subbasins. Give either ``outlets`` or ``subbasins``.
        names
            The subbasin names, one per outlet or polygon (overrides the file's
            names). Default: ``subbasin_1``, ``subbasin_2``, ...
        snap_distance
            The maximum distance [m] to move an outlet onto the stream network, and
            to match an outlet with the ``river_network`` lines. Default: 10 DEM
            cells.
        stream_threshold_area
            The drained area [m²] from which a DEM cell counts as a stream, used to
            snap the outlets (and to trace the reaches) when no ``river_network``
            is given. Default: 1 km².
        river_network
            Optional river network (a path to a line vector file, or a
            GeoDataFrame): the outlets are then snapped to the DEM cells crossed by
            these lines rather than to the accumulation-based streams, and the reach
            lengths are measured along these lines.
        reach_lengths
            Where the reach lengths come from: ``"auto"`` (default) measures them along
            the ``river_network`` lines when one is given, and along the D8 flow path
            otherwise; ``"flow_path"`` always uses the D8 flow path. The flow path of a
            coarse DEM misses the meanders the mapped river has, which shortens the
            reach and, the slope being the drop over that length, steepens it; the
            Muskingum-Cunge scheme has no calibrated celerity to absorb the error.
        outlet_name
            The name of the subbasin added for the part of the catchment not
            drained by any outlet, or not covered by any polygon (the catchment
            outlet), when needed.
        subbasins
            The subbasin polygons (a path to a polygon vector file, or a
            GeoDataFrame), in place of ``outlets``. A sliver of the catchment left
            uncovered (less than 0.5% of its area) goes to the nearest subbasin; a
            larger uncovered part becomes the ``outlet_name`` subbasin.
        id_column
            The column of the polygons holding their IDs, which ``downstream_column``
            refers to; kept in the table as ``source_id``. Default: the polygons are
            numbered 1, 2, ... in the file order.
        downstream_column
            The column of the polygons holding the ID of the polygon each drains into
            (0, empty, or an ID absent from the file for the catchment outlet).
            Default: the topology is found on the DEM.
        name_column
            The column of the polygons holding the subbasin names (default:
            ``name``, used when present).

        Returns
        -------
        The subbasin table (set on the hydro units once they are created): one row per
        subbasin with ``id``, ``downstream`` (0 for the catchment outlet), ``name``,
        ``area_local`` and ``area_drained`` [m²], ``length`` [m] and ``slope``
        [m/m] of the reach (the slope is the elevation drop over that length, so the
        product of the two is the drop whichever length is used),
        ``elevation_outlet`` [m], and the outlet coordinates ``outlet_x``,
        ``outlet_y``.
        """
        if (outlets is None) == (subbasins is None):
            raise ConfigurationError(
                "Give either outlet points (outlets) or subbasin polygons (subbasins).",
                reason="Invalid option",
            )
        if reach_lengths not in ("auto", "flow_path"):
            raise ConfigurationError(
                f"Unknown reach_lengths '{reach_lengths}': expected 'auto' or "
                f"'flow_path'.",
                reason="Invalid option",
            )
        # The flow directions are needed for outlet points, and for the reach lengths
        # unless they are measured along a river network.
        use_flow_directions = (
            subbasins is None or river_network is None or reach_lengths == "flow_path"
        )
        self._check_dependencies(use_flow_directions)
        if self.catchment.dem is None or self.catchment.dem_data is None:
            raise ConfigurationError(
                "No DEM to delineate the subbasins: call extract_dem() first.",
                reason="Missing required DEM data",
            )
        if self.catchment.map_unit_ids is not None:
            raise ConfigurationError(
                "The catchment already has hydro units: delineate the subbasins "
                "first, then create the hydro units (discretize_by, "
                "create_elevation_bands or load_unit_ids_from_raster), so that no "
                "hydro unit spans two subbasins.",
                reason="Hydro units defined before the subbasins",
            )

        rivers = self._read_river_network(river_network)
        inside = ~np.isnan(self.catchment.dem_data)
        pixel_area = self.catchment.get_dem_pixel_area()
        res = float(self.catchment.dem.res[0])
        if snap_distance is None:
            snap_distance = 10 * res
        grid, fdir, acc = None, None, None
        if use_flow_directions:
            grid, fdir, acc = self._flow_grid()
            elevation = self._elevation
        else:
            elevation = self.catchment.dem_data

        extra: dict[str, list] = {}
        if subbasins is None:
            names, pixels, upstream, downstream, local = self._from_outlets(
                outlets,
                names,
                snap_distance,
                stream_threshold_area,
                rivers,
                inside,
                outlet_name,
            )
        else:
            names, pixels, upstream, downstream, local, extra = self._from_polygons(
                subbasins,
                names,
                id_column,
                downstream_column,
                name_column,
                inside,
                elevation,
                fdir,
                acc,
                outlet_name,
            )
        n = len(names)

        # The subbasin id map.
        map_ids = np.zeros(inside.shape, dtype=np.uint16)
        for k in range(n):
            map_ids[local[k]] = k + 1
        self.map_subbasin_ids = map_ids
        self.catchment.map_subbasin_ids = map_ids

        # The reaches: from the entry of the upstream subbasins to the outlet.
        graph = None
        if rivers is not None and reach_lengths == "auto":
            graph = self._river_graph(rivers)
        rows = []
        for k in range(n):
            entries = [j for j in range(n) if downstream[j] == k + 1]
            if fdir is None:
                length, top = self._river_reach(
                    graph, pixels, k, entries, upstream[k], names, snap_distance
                )
                z_top = float(elevation[top])
            elif entries:
                length, z_top, top = 0.0, float(elevation[pixels[k]]), pixels[k]
                for j in entries:
                    path_length = self._trace_length(
                        fdir, pixels[j], pixels[k], res, within=upstream[k]
                    )
                    if path_length > length:
                        length = path_length
                        z_top = float(elevation[pixels[j]])
                        top = pixels[j]
            else:
                length, z_top, top = self._longest_flow_path(
                    grid, fdir, pixels[k], local[k], res
                )
            if fdir is not None and graph is not None:
                length = self._reach_length(
                    graph, top, pixels[k], length, names[k], snap_distance
                )
            z_out = float(elevation[pixels[k]])
            slope = max((z_top - z_out) / length, MIN_SLOPE) if length > 0 else 0.0
            x, y = self.catchment.dem.xy(*pixels[k])
            row = {
                "id": k + 1,
                "downstream": downstream[k],
                "name": names[k],
                "area_local": float(local[k].sum()) * pixel_area,
                "area_drained": float(upstream[k].sum()) * pixel_area,
                "length": round(length, 1),
                "slope": round(slope, 6),
                "elevation_outlet": round(z_out, 1),
                "outlet_x": float(x),
                "outlet_y": float(y),
            }
            for column, values in extra.items():
                row[column] = values[k]
            rows.append(row)
        table = pd.DataFrame(rows)
        self.table = table
        logger.info(
            "Delineated %d subbasins: %s",
            n,
            ", ".join(f"{r['name']} ({r['area_drained'] / 1e6:.1f} km2)" for r in rows),
        )
        return table

    def save_subbasin_ids_raster(
        self, output_path: str | Path, output_filename: str = "subbasin_ids.tif"
    ) -> None:
        """
        Save the subbasin ids raster (the DEM grid, 0 outside the catchment).

        Parameters
        ----------
        output_path
            Directory of the output file.
        output_filename
            Name of the output file. Default: ``subbasin_ids.tif``.
        """
        if self.map_subbasin_ids is None:
            raise DataError(
                "No subbasin ids raster to save: delineate the subbasins first.",
                data_type="subbasin ids raster",
                reason="Not initialized",
            )
        profile = self.catchment.dem.profile
        profile.update(dtype=rasterio.uint16, count=1, compress="lzw", nodata=0)
        with rasterio.open(Path(output_path) / output_filename, "w", **profile) as dst:
            dst.write(self.map_subbasin_ids, 1)

    # --- Internals --------------------------------------------------------------------

    @staticmethod
    def _check_dependencies(use_flow_directions: bool = True) -> None:
        if use_flow_directions and not HAS_PYSHEDS:
            raise DependencyError(
                "pysheds is required to delineate the subbasins from outlets, or to "
                "measure the reaches without a river network.",
                package_name="pysheds",
                operation="CatchmentNetwork.delineate",
                install_command="pip install pysheds",
            )
        if not HAS_RASTERIO or not HAS_GEOPANDAS:
            raise DependencyError(
                "rasterio and geopandas are required to delineate the subbasins.",
                package_name="rasterio, geopandas",
                operation="CatchmentNetwork.delineate",
                install_command="pip install rasterio geopandas",
            )

    def _read_outlets(
        self, outlets: Any, names: list[str] | None
    ) -> tuple[list[tuple[float, float]], list[str]]:
        """The outlet coordinates in the catchment CRS, and their names."""
        file_names: list[str] | None = None
        if isinstance(outlets, (str, Path)):
            outlets = gpd.read_file(outlets)
        if isinstance(outlets, dict):
            file_names = [str(k) for k in outlets]
            outlets = list(outlets.values())
        if HAS_GEOPANDAS and isinstance(outlets, gpd.GeoDataFrame):
            gdf = outlets
            if gdf.crs is not None and self.catchment.crs is not None:
                gdf = gdf.to_crs(self.catchment.crs)
            if "name" in gdf.columns:
                file_names = [str(v) for v in gdf["name"]]
            points = [(float(g.x), float(g.y)) for g in gdf.geometry]
        else:
            points = [(float(p[0]), float(p[1])) for p in outlets]
        if not points:
            raise DataError(
                "No outlet given.", data_type="outlets", reason="Empty outlets"
            )
        if names is None:
            names = file_names or [f"subbasin_{i + 1}" for i in range(len(points))]
        if len(names) != len(points):
            raise DataError(
                f"{len(names)} names for {len(points)} outlets.",
                data_type="outlets",
                reason="Names and outlets mismatch",
            )
        return points, [str(n) for n in names]

    def _from_outlets(
        self,
        outlets: Any,
        names: list[str] | None,
        snap_distance: float,
        stream_threshold_area: float,
        rivers: Any,
        inside: np.ndarray,
        outlet_name: str,
    ) -> tuple[list, list, list, list, list]:
        """The subbasins draining to outlet points, on the D8 flow directions."""
        grid, fdir, acc = self._flow_grid()
        points, names = self._read_outlets(outlets, names)
        stream = self._stream_mask(acc, stream_threshold_area, rivers)

        # Snap the outlets to the streams and delineate their upstream areas.
        pixels: list[tuple[int, int]] = []
        upstream: list[np.ndarray] = []
        for (x, y), name in zip(points, names):
            row, col = self._snap(x, y, stream, acc, snap_distance)
            pixels.append((row, col))
            with warnings.catch_warnings():
                # pysheds still calls deprecated numpy helpers (in1d).
                warnings.simplefilter("ignore", DeprecationWarning)
                mask = grid.catchment(
                    x=col, y=row, fdir=fdir, xytype="index", routing="d8"
                )
            mask = mask.view(np.ndarray).astype(bool) & inside
            if not mask.any():
                raise DataError(
                    f"The outlet '{name}' drains no cell of the catchment.",
                    data_type="outlets",
                    reason="Empty subbasin",
                )
            upstream.append(mask)

        # The tree: each outlet's immediate downstream is the smallest upstream area
        # (of another outlet) containing its pixel.
        n = len(pixels)
        sizes = [int(m.sum()) for m in upstream]
        downstream = [0] * n
        for k in range(n):
            row, col = pixels[k]
            containers = [
                j
                for j in range(n)
                if j != k and upstream[j][row, col] and sizes[j] > sizes[k]
            ]
            if containers:
                downstream[k] = min(containers, key=lambda j: sizes[j]) + 1

        # The local area of each subbasin: its upstream area minus the nested ones.
        local: list[np.ndarray] = []
        for k in range(n):
            mask = upstream[k].copy()
            for j in range(n):
                if j != k and downstream[j] == k + 1:
                    mask &= ~upstream[j]
            local.append(mask)

        # The catchment outlet: an outlet draining (nearly) the whole catchment, or an
        # extra subbasin for what no outlet drains.
        covered = np.zeros_like(inside)
        for mask in upstream:
            covered |= mask
        remainder = inside & ~covered
        terminals = [k for k in range(n) if downstream[k] == 0]
        if len(terminals) == 1 and remainder.sum() < 0.005 * inside.sum():
            local[terminals[0]] |= remainder
            upstream[terminals[0]] |= remainder
        else:
            names = list(names) + [outlet_name]
            local.append(remainder)
            upstream.append(inside.copy())
            outlet_row, outlet_col = np.unravel_index(
                np.argmax(np.where(inside, acc, -1)), acc.shape
            )
            pixels.append((int(outlet_row), int(outlet_col)))
            for k in terminals:
                downstream[k] = n + 1
            downstream.append(0)

        return list(names), pixels, upstream, downstream, local

    def _from_polygons(
        self,
        subbasins: Any,
        names: list[str] | None,
        id_column: str | None,
        downstream_column: str | None,
        name_column: str,
        inside: np.ndarray,
        elevation: np.ndarray,
        fdir: Any,
        acc: np.ndarray | None,
        outlet_name: str,
    ) -> tuple[list, list, list, list, list, dict]:
        """The subbasins from polygons: local areas, nested drained areas, or both."""
        from rasterio.features import geometry_mask

        gdf = self._read_polygons(subbasins)
        for column in (id_column, downstream_column):
            if column is not None and column not in gdf.columns:
                raise DataError(
                    f"The subbasin polygons have no '{column}' column.",
                    data_type="subbasins",
                    reason="Missing column",
                )
        n = len(gdf)
        if names is None:
            if name_column in gdf.columns:
                names = [str(v) for v in gdf[name_column]]
            else:
                names = [f"subbasin_{i + 1}" for i in range(n)]
        if len(names) != n:
            raise DataError(
                f"{len(names)} names for {n} subbasin polygons.",
                data_type="subbasins",
                reason="Names and polygons mismatch",
            )
        names = [str(v) for v in names]

        # Each polygon on the DEM grid, within the catchment.
        polygons: list[np.ndarray] = []
        for name, geometry in zip(names, gdf.geometry):
            mask = (
                ~geometry_mask(
                    [geometry],
                    transform=self.catchment.dem.transform,
                    out_shape=inside.shape,
                )
                & inside
            )
            if not mask.any():
                raise DataError(
                    f"The subbasin polygon '{name}' covers no cell of the catchment.",
                    data_type="subbasins",
                    reason="Empty subbasin",
                )
            polygons.append(mask)

        # Every cell goes to the smallest polygon covering it: the larger polygons are
        # painted first, so that a nested drained area is carved out of the polygons
        # containing it, while the polygons of a partition keep their own cells.
        owner = np.zeros(inside.shape, dtype=np.int32)
        for k in sorted(range(n), key=lambda i: -int(polygons[i].sum())):
            owner[polygons[k]] = k + 1
        remainder = inside & (owner == 0)
        if remainder.sum() >= 0.005 * inside.sum():
            names.append(outlet_name)
            polygons.append(remainder)
            owner[remainder] = n + 1
            n += 1
        elif remainder.any():
            logger.warning(
                "The subbasin polygons leave %.2f%% of the catchment uncovered; these "
                "cells go to the nearest subbasin.",
                100 * remainder.sum() / inside.sum(),
            )
            owner = self._fill_nearest(owner, remainder)
        local = [owner == k + 1 for k in range(n)]
        for k in range(n):
            if not local[k].any():
                raise DataError(
                    f"The subbasin polygon '{names[k]}' is entirely covered by smaller "
                    "polygons and keeps no cell of its own.",
                    data_type="subbasins",
                    reason="Empty subbasin",
                )

        # The outlet of each polygon: the cell of largest flow accumulation when the
        # flow directions are computed, its lowest cell otherwise.
        pixels = [self._outlet_cell(polygons[k], elevation, acc) for k in range(n)]

        if downstream_column is not None:
            downstream = self._downstream_from_column(
                gdf, id_column, downstream_column, n, names
            )
        else:
            downstream = self._downstream_from_dem(
                pixels, polygons, owner, inside, elevation, fdir, acc, names
            )
        self._check_tree(downstream, names)

        # The drained area of each subbasin: its own and all its upstream ones.
        upstream = [mask.copy() for mask in local]
        for k in range(n):
            j = downstream[k]
            while j:
                upstream[j - 1] |= local[k]
                j = downstream[j - 1]
        # The outlets of the nested drained areas, now complete.
        pixels = [self._outlet_cell(upstream[k], elevation, acc) for k in range(n)]

        extra: dict[str, list] = {}
        if id_column is not None:
            extra["source_id"] = list(gdf[id_column]) + [None] * (n - len(gdf))
        return names, pixels, upstream, downstream, local, extra

    def _read_polygons(self, subbasins: Any) -> Any:
        """The subbasin polygons as a GeoDataFrame in the catchment CRS."""
        if isinstance(subbasins, (str, Path)):
            subbasins = gpd.read_file(subbasins)
        if not isinstance(subbasins, gpd.GeoDataFrame) or subbasins.empty:
            raise DataError(
                "The subbasins must be given as a polygon vector file or a "
                "non-empty GeoDataFrame.",
                data_type="subbasins",
                reason="Unsupported or empty subbasins",
            )
        if subbasins.crs is not None and self.catchment.crs is not None:
            subbasins = subbasins.to_crs(self.catchment.crs)
        return subbasins.reset_index(drop=True)

    @staticmethod
    def _outlet_cell(
        mask: np.ndarray, elevation: np.ndarray, acc: np.ndarray | None
    ) -> tuple[int, int]:
        """The outlet cell of an area: largest accumulation, or lowest elevation."""
        if acc is not None:
            values = np.where(mask, acc, -np.inf)
            index = int(np.argmax(values))
        else:
            values = np.where(mask, elevation, np.inf)
            index = int(np.nanargmin(values))
        row, col = np.unravel_index(index, mask.shape)
        return int(row), int(col)

    @staticmethod
    def _downstream_from_column(
        gdf: Any,
        id_column: str | None,
        downstream_column: str,
        n: int,
        names: list[str],
    ) -> list[int]:
        """The topology from the polygons' downstream IDs (an outlet subbasin last)."""
        if id_column is not None:
            ids = list(gdf[id_column])
        else:
            ids = list(range(1, len(gdf) + 1))
        if len(set(ids)) != len(ids):
            raise DataError(
                "The subbasin polygon IDs must be unique.",
                data_type="subbasins",
                reason="Duplicate subbasin ID",
            )
        index = {value: i for i, value in enumerate(ids)}
        downstream = []
        for value in gdf[downstream_column]:
            target = index.get(value) if not pd.isna(value) else None
            downstream.append(target + 1 if target is not None else 0)
        if n > len(gdf):
            # The outlet subbasin added for the uncovered part receives the polygons
            # draining out of the set.
            downstream = [d if d else n for d in downstream] + [0]
        return downstream

    def _downstream_from_dem(
        self,
        pixels: list[tuple[int, int]],
        polygons: list[np.ndarray],
        owner: np.ndarray,
        inside: np.ndarray,
        elevation: np.ndarray,
        fdir: Any,
        acc: np.ndarray | None,
        names: list[str],
    ) -> list[int]:
        """
        The topology found on the DEM: each polygon drains into the subbasin owning
        the cell downstream of its outlet (the D8 receiver when the flow directions
        are computed, its lowest neighbour outside the polygon otherwise). The
        subbasin holding the catchment outlet drains out of the catchment.
        """
        n = len(pixels)
        terminal = int(owner[self._outlet_cell(inside, elevation, acc)]) - 1
        codes = fdir.view(np.ndarray) if fdir is not None else None
        rows, cols = inside.shape
        downstream = [0] * n
        for k in range(n):
            if k == terminal:
                continue
            row, col = pixels[k]
            receiver = None
            if codes is not None:
                offset = _D8_OFFSETS.get(int(codes[row, col]))
                if offset is not None:
                    r, c = row + offset[0], col + offset[1]
                    if 0 <= r < rows and 0 <= c < cols and owner[r, c] > 0:
                        receiver = (r, c)
            if receiver is None or owner[receiver] == k + 1:
                # The lowest cell outside the polygon in the smallest window around
                # the outlet holding one (an outline usually runs a few cells beyond
                # the outlet).
                receiver = None
                for radius in range(1, OUTLET_SEARCH_RADIUS + 1):
                    r0, r1 = max(0, row - radius), min(rows, row + radius + 1)
                    c0, c1 = max(0, col - radius), min(cols, col + radius + 1)
                    candidates = inside[r0:r1, c0:c1] & ~polygons[k][r0:r1, c0:c1]
                    if candidates.any():
                        window = np.where(candidates, elevation[r0:r1, c0:c1], np.inf)
                        r, c = np.unravel_index(int(np.argmin(window)), window.shape)
                        receiver = (int(r) + r0, int(c) + c0)
                        break
            if receiver is None or owner[receiver] in (0, k + 1):
                raise DataError(
                    f"Cannot tell from the DEM which subbasin '{names[k]}' drains "
                    "into: its outlet touches no other subbasin. Give the topology "
                    "with downstream_column.",
                    data_type="subbasins",
                    reason="Undetermined topology",
                )
            downstream[k] = int(owner[receiver])
        return downstream

    @staticmethod
    def _check_tree(downstream: list[int], names: list[str]) -> None:
        """Check that the subbasins form a single tree draining to one outlet."""
        n = len(downstream)
        terminals = [names[k] for k in range(n) if downstream[k] == 0]
        if len(terminals) != 1:
            raise DataError(
                f"The subbasins drain to {len(terminals)} outlets "
                f"({', '.join(terminals)}) instead of one.",
                data_type="subbasins",
                reason="Not a single tree",
            )
        for k in range(n):
            j, steps = downstream[k], 0
            while j:
                j = downstream[j - 1]
                steps += 1
                if steps > n:
                    raise DataError(
                        f"The subbasin '{names[k]}' drains into a cycle; give the "
                        "topology with downstream_column.",
                        data_type="subbasins",
                        reason="Cycle in the network",
                    )

    @staticmethod
    def _fill_nearest(owner: np.ndarray, todo: np.ndarray) -> np.ndarray:
        """Give the cells to fill the id of a neighbouring cell, growing inwards."""
        owner = owner.copy()
        todo = todo.copy()
        rows, cols = owner.shape
        while todo.any():
            grown = np.zeros_like(owner)
            for dr in (-1, 0, 1):
                for dc in (-1, 0, 1):
                    if dr == 0 and dc == 0:
                        continue
                    shifted = np.zeros_like(owner)
                    shifted[
                        max(dr, 0) : rows + min(dr, 0), max(dc, 0) : cols + min(dc, 0)
                    ] = owner[
                        max(-dr, 0) : rows + min(-dr, 0),
                        max(-dc, 0) : cols + min(-dc, 0),
                    ]
                    grown = np.where(grown == 0, shifted, grown)
            fill = todo & (grown > 0)
            if not fill.any():
                break  # cells cut off from every subbasin: left as they are
            owner[fill] = grown[fill]
            todo &= ~fill
        return owner

    def _river_reach(
        self,
        graph: tuple[list, dict] | None,
        pixels: list[tuple[int, int]],
        k: int,
        entries: list[int],
        upstream: np.ndarray,
        names: list[str],
        max_distance: float,
    ) -> tuple[float, tuple[int, int]]:
        """
        The reach length [m] along the river network, and the cell at its top, for
        polygons without flow directions: from the farthest upstream outlet, or for a
        headwater subbasin from the farthest point of the network within it.
        """
        xy = self.catchment.dem.xy
        outlet = xy(*pixels[k])
        if entries:
            length, top = -1.0, pixels[k]
            for j in entries:
                start = xy(*pixels[j])
                path = self._network_length(graph, start, outlet, max_distance)
                if path is None:
                    path = math.hypot(start[0] - outlet[0], start[1] - outlet[1])
                    logger.warning(
                        "Subbasin %s: no path along the river network from the outlet "
                        "of %s; using the straight-line distance (%.0f m).",
                        names[k],
                        names[j],
                        path,
                    )
                if path > length:
                    length, top = path, pixels[j]
            return length, top

        lines, adjacency = graph
        index, along, offset = self._project_on_network(lines, *outlet)
        length, top = 0.0, pixels[k]
        if index >= 0 and offset <= max_distance:
            inverse = ~self.catchment.dem.transform
            distances = self._graph_distances(
                adjacency, self._line_ends(lines[index], along)
            )
            for node, distance in distances.items():
                col, row = inverse * (
                    node[0] * NODE_TOLERANCE,
                    node[1] * NODE_TOLERANCE,
                )
                row, col = int(np.floor(row)), int(np.floor(col))
                if not (0 <= row < upstream.shape[0] and 0 <= col < upstream.shape[1]):
                    continue
                if upstream[row, col] and distance > length:
                    length, top = distance, (row, col)
        if length <= 0:
            logger.warning(
                "Subbasin %s: the river network does not reach into it; its reach "
                "has no length (instantaneous routing).",
                names[k],
            )
        return length, top

    def _flow_grid(self) -> tuple[Any, Any, np.ndarray]:
        """The pysheds grid, D8 flow directions and accumulation of the DEM (cached)."""
        if self._grid is None:
            dem_path = self.catchment.dem.files[0]
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", DeprecationWarning)
                grid = pyshedsGrid.from_raster(dem_path)
                dem = grid.read_raster(dem_path)
                filled = grid.fill_depressions(grid.fill_pits(dem))
                inflated = grid.resolve_flats(filled)
                fdir = grid.flowdir(inflated, routing="d8", nodata_out=np.int64(0))
                acc = grid.accumulation(fdir, routing="d8")
            self._grid = grid
            self._fdir = fdir
            self._acc = acc.view(np.ndarray).astype(float)
            self._elevation = inflated.view(np.ndarray).astype(float)
        return self._grid, self._fdir, self._acc

    def _stream_mask(
        self, acc: np.ndarray, threshold_area: float, river_network: Any
    ) -> np.ndarray:
        """The DEM cells that are streams: from the river lines, or the accumulation."""
        if river_network is not None:
            masked = self.catchment.mask_dem(
                river_network, nodata=-9999, all_touched=True
            )
            return masked != -9999
        threshold_cells = max(1.0, threshold_area / self.catchment.get_dem_pixel_area())
        return acc >= threshold_cells

    def _snap(
        self, x: float, y: float, stream: np.ndarray, acc: np.ndarray, distance: float
    ) -> tuple[int, int]:
        """The nearest stream cell to a point, within the snapping distance."""
        col, row = ~self.catchment.dem.transform * (x, y)
        row, col = int(np.floor(row)), int(np.floor(col))
        res = float(self.catchment.dem.res[0])
        radius = int(np.ceil(distance / res))
        r0, r1 = max(0, row - radius), min(stream.shape[0], row + radius + 1)
        c0, c1 = max(0, col - radius), min(stream.shape[1], col + radius + 1)
        window = stream[r0:r1, c0:c1]
        if not window.any():
            raise DataError(
                f"No stream cell within {distance:.0f} m of the outlet ({x:.0f}, "
                f"{y:.0f}); increase snap_distance or lower stream_threshold_area.",
                data_type="outlets",
                reason="Outlet away from the streams",
            )
        rows, cols = np.nonzero(window)
        rows, cols = rows + r0, cols + c0
        dist = np.hypot(rows - row, cols - col) * res
        # The nearest stream cell; among cells at the same distance, the largest stream.
        order = np.lexsort((-acc[rows, cols], np.round(dist, 6)))
        best = order[0]
        if dist[best] > distance:
            raise DataError(
                f"No stream cell within {distance:.0f} m of the outlet ({x:.0f}, "
                f"{y:.0f}); increase snap_distance or lower stream_threshold_area.",
                data_type="outlets",
                reason="Outlet away from the streams",
            )
        return int(rows[best]), int(cols[best])

    def _read_river_network(self, river_network: Any) -> Any:
        """The river network as a GeoDataFrame in the catchment CRS."""
        if river_network is None:
            return None
        if isinstance(river_network, (str, Path)):
            river_network = gpd.read_file(river_network)
        if river_network.crs is not None and self.catchment.crs is not None:
            river_network = river_network.to_crs(self.catchment.crs)
        return river_network

    @staticmethod
    def _river_graph(river_network: Any) -> tuple[list, dict]:
        """
        The river lines and the adjacency of their ends.

        The lines are the edges of the network; two ends closer than
        :data:`NODE_TOLERANCE` are the same node. A network that is not noded at its
        confluences gives a disconnected graph, and the reaches crossing a gap keep
        their flow path length.
        """
        lines = []
        for geom in river_network.geometry:
            if geom is None or geom.is_empty:
                continue
            if geom.geom_type == "LineString":
                lines.append(geom)
            elif geom.geom_type == "MultiLineString":
                lines.extend(geom.geoms)

        def node(coord):
            return (
                round(coord[0] / NODE_TOLERANCE),
                round(coord[1] / NODE_TOLERANCE),
            )

        adjacency: dict = {}
        for line in lines:
            coords = line.coords
            start, end = node(coords[0]), node(coords[-1])
            if start == end:
                continue
            adjacency.setdefault(start, []).append((end, line.length))
            adjacency.setdefault(end, []).append((start, line.length))
        return lines, adjacency

    @staticmethod
    def _project_on_network(
        lines: list, x: float, y: float
    ) -> tuple[int, float, float]:
        """The nearest line, the distance along it, and the distance to it."""
        from shapely.geometry import Point

        point = Point(x, y)
        best, best_distance = -1, math.inf
        for i, line in enumerate(lines):
            distance = line.distance(point)
            if distance < best_distance:
                best, best_distance = i, distance
        if best < 0:
            return -1, 0.0, math.inf
        return best, float(lines[best].project(point)), float(best_distance)

    @staticmethod
    def _graph_distance(adjacency: dict, sources: dict, targets: dict) -> float | None:
        """The shortest distance from any source to any target, extra costs included."""
        best = math.inf
        settled: dict = {}
        heap = [(cost, node) for node, cost in sources.items()]
        heapq.heapify(heap)
        while heap:
            cost, node = heapq.heappop(heap)
            if cost >= best:
                break
            if node in settled:
                continue
            settled[node] = cost
            if node in targets:
                best = min(best, cost + targets[node])
            for other, weight in adjacency.get(node, ()):
                if other not in settled:
                    heapq.heappush(heap, (cost + weight, other))
        return None if math.isinf(best) else best

    def _network_length(
        self,
        graph: tuple[list, dict],
        top: tuple[float, float],
        outlet: tuple[float, float],
        max_distance: float,
    ) -> float | None:
        """The distance along the river network between two points (None if no path)."""
        lines, adjacency = graph
        i_top, d_top, off_top = self._project_on_network(lines, *top)
        i_out, d_out, off_out = self._project_on_network(lines, *outlet)
        if i_top < 0 or i_out < 0 or max(off_top, off_out) > max_distance:
            return None
        if i_top == i_out:
            return abs(d_top - d_out)

        return self._graph_distance(
            adjacency,
            self._line_ends(lines[i_top], d_top),
            self._line_ends(lines[i_out], d_out),
        )

    @staticmethod
    def _line_ends(line: Any, along: float) -> dict:
        """The two end nodes of a line and their distances from a point along it."""
        coords = line.coords
        start = (
            round(coords[0][0] / NODE_TOLERANCE),
            round(coords[0][1] / NODE_TOLERANCE),
        )
        end = (
            round(coords[-1][0] / NODE_TOLERANCE),
            round(coords[-1][1] / NODE_TOLERANCE),
        )
        return {start: along, end: max(line.length - along, 0.0)}

    @staticmethod
    def _graph_distances(adjacency: dict, sources: dict) -> dict:
        """The shortest distance from the sources to every node reached."""
        settled: dict = {}
        heap = [(cost, node) for node, cost in sources.items()]
        heapq.heapify(heap)
        while heap:
            cost, node = heapq.heappop(heap)
            if node in settled:
                continue
            settled[node] = cost
            for other, weight in adjacency.get(node, ()):
                if other not in settled:
                    heapq.heappush(heap, (cost + weight, other))
        return settled

    def _reach_length(
        self,
        graph: tuple[list, dict],
        top: tuple[int, int],
        outlet: tuple[int, int],
        flow_path_length: float,
        name: str,
        max_distance: float,
    ) -> float:
        """The reach length [m] along the river network, or the flow path length."""
        if flow_path_length <= 0:
            return flow_path_length
        length = self._network_length(
            graph,
            self.catchment.dem.xy(*top),
            self.catchment.dem.xy(*outlet),
            max_distance,
        )
        if length is None or length <= 0:
            logger.warning(
                "Subbasin %s: no path along the river network between the ends of its "
                "reach; keeping the flow path length (%.0f m).",
                name,
                flow_path_length,
            )
            return flow_path_length
        ratio = length / flow_path_length
        low, high = LENGTH_RATIO_BOUNDS
        if not low <= ratio <= high:
            logger.warning(
                "Subbasin %s: the river network gives a reach length of %.0f m for a "
                "flow path of %.0f m (ratio %.2f); keeping the flow path length.",
                name,
                length,
                flow_path_length,
                ratio,
            )
            return flow_path_length
        return length

    @staticmethod
    def _trace_length(
        fdir: Any,
        start: tuple[int, int],
        end: tuple[int, int],
        res: float,
        within: np.ndarray | None = None,
    ) -> float:
        """
        The D8 flow path length [m] from a cell to another (0 if not reached). With
        ``within``, a path leaving that area before reaching the end cell (a subbasin
        polygon slightly off the DEM drainage) stops there, and its length so far is
        returned.
        """
        codes = fdir.view(np.ndarray)
        row, col = start
        length = 0.0
        for _ in range(codes.size):
            if (row, col) == end:
                return length
            offset = _D8_OFFSETS.get(int(codes[row, col]))
            if offset is None:
                return 0.0
            step = res * (np.sqrt(2.0) if offset[0] and offset[1] else 1.0)
            row, col = row + offset[0], col + offset[1]
            if not (0 <= row < codes.shape[0] and 0 <= col < codes.shape[1]):
                return 0.0
            if within is not None and not within[row, col]:
                return length
            length += step
        return 0.0

    def _longest_flow_path(
        self,
        grid: Any,
        fdir: Any,
        outlet: tuple[int, int],
        local: np.ndarray,
        res: float,
    ) -> tuple[float, float, tuple[int, int]]:
        """The longest flow path of a headwater subbasin: length, elevation, cell."""
        row, col = outlet
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", DeprecationWarning)
            distances = grid.distance_to_outlet(
                x=col, y=row, fdir=fdir, xytype="index", routing="d8"
            )
        distances = distances.view(np.ndarray).astype(float)
        distances[~local] = -1
        distances[~np.isfinite(distances)] = -1
        far = np.unravel_index(int(np.argmax(distances)), distances.shape)
        cell = (int(far[0]), int(far[1]))
        length = self._trace_length(fdir, cell, outlet, res)
        return length, float(self._elevation[far]), cell

    def assign_hydro_units(self) -> None:
        """
        Give every hydro unit its subbasin, from the subbasin ids raster.

        Called when the hydro units are created after the delineation from a unit
        raster (by :meth:`Catchment.get_hydro_units_attributes
        <hydrobricks.Catchment.get_hydro_units_attributes>`); the discretization
        sets the subbasins itself. The subbasin table is set on the hydro units.

        Raises
        ------
        DataError
            If a hydro unit spans several subbasins, or lies outside all of them.
        """
        catchment = self.catchment
        map_ids = catchment.map_subbasin_ids
        unit_map = catchment.map_unit_ids
        unit_ids = catchment.hydro_units.hydro_units["id"].iloc[:, 0].to_numpy()
        values: list[int] = []
        spanning: list[str] = []
        outside: list[int] = []
        for unit_id in unit_ids:
            cells = map_ids[unit_map == unit_id]
            cells = cells[cells > 0]
            if cells.size == 0:
                outside.append(int(unit_id))
                values.append(0)
                continue
            counts = np.bincount(cells)
            best = int(np.argmax(counts))
            values.append(best)
            share = counts[best] / cells.size
            if share < 1.0:
                spanning.append(f"{int(unit_id)} ({100 * (1 - share):.0f}% elsewhere)")
        if spanning:
            raise DataError(
                f"{len(spanning)} hydro unit(s) span several subbasins: "
                + ", ".join(spanning[:10])
                + (", ..." if len(spanning) > 10 else "")
                + ". Create the hydro units within the subbasins (discretize_by).",
                data_type="hydro units",
                reason="Hydro unit spanning several subbasins",
            )
        if outside:
            raise DataError(
                f"Hydro unit(s) {outside[:10]} lie outside the subbasins.",
                data_type="hydro units",
                reason="Unit outside the subbasins",
            )
        self._set_subbasin_column(np.array(values, dtype=int))
        if self.table is not None:
            catchment.hydro_units.set_subbasins(self.table)

    def _set_subbasin_column(self, values: np.ndarray) -> None:
        hydro_units = self.catchment.hydro_units
        column = hydro_units.SUBBASIN_COLUMN
        if hydro_units.has(column):
            hydro_units.hydro_units[(column, "-")] = values
        else:
            hydro_units.add_property((column, "-"), values)
