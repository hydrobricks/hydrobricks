"""Delineation of the subbasins of a river network from outlet points on the DEM."""

from __future__ import annotations

import logging
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


class CatchmentNetwork:
    """
    Delineate the subbasins of a river network from outlet points (gauges).

    Each outlet is snapped to the stream network derived from the DEM flow
    accumulation (or to a given river network), its upstream area is delineated
    with the D8 flow directions (``pysheds``), and the nested areas are carved out
    so that every subbasin holds only its own (local) area. The hydro units are
    then assigned to the subbasins, and the reach of each subbasin (the main
    channel from the entry of its upstream subbasins to its outlet) is measured
    on the DEM: length along the D8 flow path and slope from the elevations at
    its ends.

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
        outlets: Any,
        names: list[str] | None = None,
        snap_distance: float | None = None,
        stream_threshold_area: float = 1e6,
        river_network: str | Path | Any | None = None,
        split_units: bool = False,
        outlet_name: str = "outlet",
    ) -> pd.DataFrame:
        """
        Delineate the subbasins draining to the given outlets.

        Parameters
        ----------
        outlets
            The outlet points (gauges), in the catchment CRS unless a vector file or
            GeoDataFrame carries its own CRS: a path to a point vector file, a
            GeoDataFrame of points, a list of ``(x, y)`` pairs, or a mapping of
            name to ``(x, y)``. A ``name`` column of a vector file names the
            subbasins.
        names
            The subbasin names, one per outlet (overrides the file's ``name``
            column). Default: ``subbasin_1``, ``subbasin_2``, ...
        snap_distance
            The maximum distance [m] to move an outlet onto the stream network.
            Default: 10 DEM cells.
        stream_threshold_area
            The drained area [m²] from which a DEM cell counts as a stream, used to
            snap the outlets (and to trace the reaches) when no ``river_network``
            is given. Default: 1 km².
        river_network
            Optional river network (a path to a line vector file, or a
            GeoDataFrame): the outlets are then snapped to the DEM cells crossed by
            these lines rather than to the accumulation-based streams.
        split_units
            Whether to split the hydro units spanning several subbasins, one part
            per subbasin (default: False). A split rebuilds the hydro units from
            the DEM (area, elevation, slope, ...) and drops the properties the
            discretization had attached to them; extract the land covers again
            afterwards. Without a split, a unit is assigned to the subbasin holding
            most of its cells, and the units spanning several subbasins are listed
            in a warning.
        outlet_name
            The name of the subbasin added for the part of the catchment not
            drained by any outlet (the catchment outlet), when needed.

        Returns
        -------
        The subbasin table (also set on the catchment's hydro units): one row per
        subbasin with ``id``, ``downstream`` (0 for the catchment outlet), ``name``,
        ``area_local`` and ``area_drained`` [m²], ``length`` [m] and ``slope``
        [m/m] of the reach, ``elevation_outlet`` [m], and the snapped outlet
        coordinates ``outlet_x``, ``outlet_y``.
        """
        self._check_dependencies()
        if self.catchment.dem is None or self.catchment.dem_data is None:
            raise ConfigurationError(
                "No DEM to delineate the subbasins: call extract_dem() first.",
                reason="Missing required DEM data",
            )
        if self.catchment.map_unit_ids is None:
            raise ConfigurationError(
                "No hydro units to assign to the subbasins: discretize the catchment "
                "or load the unit ids raster first.",
                reason="Catchment not discretized",
            )

        points, names = self._read_outlets(outlets, names)
        grid, fdir, acc = self._flow_grid()
        inside = ~np.isnan(self.catchment.dem_data)
        stream = self._stream_mask(acc, stream_threshold_area, river_network)
        pixel_area = self.catchment.get_dem_pixel_area()
        res = float(self.catchment.dem.res[0])
        if snap_distance is None:
            snap_distance = 10 * res

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
            n += 1

        # The subbasin id map.
        map_ids = np.zeros(inside.shape, dtype=np.uint16)
        for k in range(n):
            map_ids[local[k]] = k + 1
        self.map_subbasin_ids = map_ids
        self.catchment.map_subbasin_ids = map_ids

        # The reaches: from the entry of the upstream subbasins to the outlet.
        elevation = self._elevation
        rows = []
        for k in range(n):
            entries = [j for j in range(n) if downstream[j] == k + 1]
            length, z_top = 0.0, float(elevation[pixels[k]])
            if entries:
                for j in entries:
                    path_length = self._trace_length(fdir, pixels[j], pixels[k], res)
                    if path_length > length:
                        length = path_length
                        z_top = float(elevation[pixels[j]])
            else:
                length, z_top = self._longest_flow_path(
                    grid, fdir, pixels[k], local[k], res
                )
            z_out = float(elevation[pixels[k]])
            slope = max((z_top - z_out) / length, MIN_SLOPE) if length > 0 else 0.0
            x, y = self.catchment.dem.xy(*pixels[k])
            rows.append(
                {
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
            )
        table = pd.DataFrame(rows)

        # The hydro units of each subbasin.
        self._assign_hydro_units(map_ids, split_units)
        self.catchment.hydro_units.set_subbasins(table)
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
    def _check_dependencies() -> None:
        if not HAS_PYSHEDS:
            raise DependencyError(
                "pysheds is required to delineate the subbasins.",
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
            if isinstance(river_network, (str, Path)):
                river_network = gpd.read_file(river_network)
            if river_network.crs is not None and self.catchment.crs is not None:
                river_network = river_network.to_crs(self.catchment.crs)
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

    @staticmethod
    def _trace_length(
        fdir: Any, start: tuple[int, int], end: tuple[int, int], res: float
    ) -> float:
        """The D8 flow path length [m] from a cell to another (0 if not reached)."""
        codes = fdir.view(np.ndarray)
        row, col = start
        length = 0.0
        for _ in range(codes.size):
            if (row, col) == end:
                return length
            offset = _D8_OFFSETS.get(int(codes[row, col]))
            if offset is None:
                return 0.0
            length += res * (np.sqrt(2.0) if offset[0] and offset[1] else 1.0)
            row, col = row + offset[0], col + offset[1]
            if not (0 <= row < codes.shape[0] and 0 <= col < codes.shape[1]):
                return 0.0
        return 0.0

    def _longest_flow_path(
        self,
        grid: Any,
        fdir: Any,
        outlet: tuple[int, int],
        local: np.ndarray,
        res: float,
    ) -> tuple[float, float]:
        """The longest flow path [m] of a headwater subbasin and its top elevation."""
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
        length = self._trace_length(fdir, (int(far[0]), int(far[1])), outlet, res)
        return length, float(self._elevation[far])

    def _assign_hydro_units(self, map_ids: np.ndarray, split_units: bool) -> None:
        """Give every hydro unit its subbasin (majority cell), or split the units."""
        catchment = self.catchment
        unit_map = catchment.map_unit_ids
        unit_ids = catchment.hydro_units.hydro_units["id"].iloc[:, 0].to_numpy()
        spanning: list[str] = []
        majority: dict[int, int] = {}
        for unit_id in unit_ids:
            cells = map_ids[unit_map == unit_id]
            cells = cells[cells > 0]
            if cells.size == 0:
                majority[int(unit_id)] = 0
                continue
            counts = np.bincount(cells)
            best = int(np.argmax(counts))
            majority[int(unit_id)] = best
            share = counts[best] / cells.size
            if share < 1.0:
                spanning.append(f"{int(unit_id)} ({100 * (1 - share):.0f}% elsewhere)")

        if split_units and spanning:
            self._split_units(map_ids)
            return

        if spanning:
            logger.warning(
                "%d hydro unit(s) span several subbasins and were assigned to the "
                "subbasin holding most of their cells: %s. Use split_units=True to "
                "split them.",
                len(spanning),
                ", ".join(spanning[:10]) + (", ..." if len(spanning) > 10 else ""),
            )
        unassigned = [u for u, s in majority.items() if s == 0]
        if unassigned:
            raise DataError(
                f"Hydro unit(s) {unassigned[:10]} have no cell in the catchment DEM "
                "and cannot be assigned to a subbasin.",
                data_type="hydro units",
                reason="Unit outside the catchment",
            )
        values = np.array([majority[int(u)] for u in unit_ids], dtype=int)
        self._set_subbasin_column(values)

    def _split_units(self, map_ids: np.ndarray) -> None:
        """Split the hydro units by subbasin and rebuild them from the DEM."""
        from hydrobricks.hydro_units import HydroUnits

        catchment = self.catchment
        unit_map = catchment.map_unit_ids
        new_map = np.zeros_like(unit_map)
        subbasins: list[int] = []
        new_id = 0
        for unit_id in np.unique(unit_map[unit_map > 0]):
            for subbasin_id in np.unique(map_ids[unit_map == unit_id]):
                if subbasin_id == 0:
                    continue
                new_id += 1
                new_map[(unit_map == unit_id) & (map_ids == subbasin_id)] = new_id
                subbasins.append(int(subbasin_id))
        max_units = int(np.iinfo(np.uint16).max)
        if new_id > max_units:
            raise DataError(
                f"Splitting the hydro units by subbasin gives {new_id} units, more "
                f"than the {max_units} the unit ids raster can hold.",
                data_type="hydro units",
                reason="Too many hydro units",
            )
        old_units = catchment.hydro_units
        catchment.map_unit_ids = new_map.astype(np.uint16)
        catchment.hydro_units = HydroUnits(
            list(old_units.land_cover_types), list(old_units.land_cover_names)
        )
        catchment.initialize_land_cover_fractions()
        catchment.get_hydro_units_attributes()
        self._set_subbasin_column(np.array(subbasins, dtype=int))
        logger.info(
            "Split the hydro units by subbasin: %d units (from %d).",
            new_id,
            len(old_units.hydro_units),
        )

    def _set_subbasin_column(self, values: np.ndarray) -> None:
        hydro_units = self.catchment.hydro_units
        column = hydro_units.SUBBASIN_COLUMN
        if hydro_units.has(column):
            hydro_units.hydro_units[(column, "-")] = values
        else:
            hydro_units.add_property((column, "-"), values)
