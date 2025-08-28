from __future__ import annotations

from typing import TYPE_CHECKING

import numpy as np
import pandas as pd
from scipy import ndimage

import hydrobricks as hb

if TYPE_CHECKING:
    from hydrobricks.catchment import Catchment


class CatchmentConnectivity:
    """
    Class to handle connectivity of catchments.
    """

    def __init__(self, catchment: Catchment):
        """
        Initialize the Connectivity class.

        Parameters
        ----------
        catchment
            The catchment object.
        """
        self.catchment = catchment

    def calculate(
            self,
            mode: str = 'multiple',
            force_connectivity: bool = False,
            precision: int = 3
    ) -> pd.DataFrame:
        """
        Calculate the connectivity between hydro units using a flow accumulation
        partition. Connectivity between hydro units is forced to stay within the
        catchment.

        Parameters
        ----------
        mode
            The mode to calculate the connectivity:
            'single' = keep the highest connectivity only
            'multiple' = keep all connectivity values (multiple connections)
        force_connectivity
            If True, all hydro units are forced to have a connection to another hydro unit.
            When there is no downstream hydro unit, the connectivity is set to the neighboring
            hydro units, proportionally to the length of the common border.
            If False, and if a hydro unit contributes mostly to surfaces out of the catchment,
            the connectivity will be nulled.
            Default is False (recommended).
        precision
            The precision of the connectivity values. Default is 3.
            This is used to round the connectivity values to a given number of decimal places.

        Returns
        -------
        The hydro units connectivity.
        """
        if not hb.has_pysheds:
            raise ImportError("pysheds is required to do this.")

        if self.catchment.dem is None:
            raise ValueError("No DEM to calculate the connectivity.")

        if self.catchment.map_unit_ids is None:
            raise ValueError("No hydro units to calculate the connectivity.")

        # Create a pysheds instance
        dem_path = self.catchment.dem.files[0]
        grid = hb.pyshedsGrid.from_raster(dem_path)
        dem = grid.read_raster(dem_path)

        # Fill pits and depressions in DEM and resolve flats
        pit_filled_dem = grid.fill_pits(dem)
        flooded_dem = grid.fill_depressions(pit_filled_dem)
        inflated_dem = grid.resolve_flats(flooded_dem)

        # Compute flow direction and flow accumulation
        flow_dir = grid.flowdir(
            inflated_dem,
            routing='d8',
            nodata_out=np.int64(0)
        )

        # Check that the hydro units are defined
        if len(self.catchment.hydro_units.hydro_units) == 0:
            raise ValueError("No hydro units defined in the catchment. "
                             "Please define the hydro units before "
                             "calculating the connectivity.")

        # Create a dataframe with the hydro units IDs and a column of empty dictionaries
        df = self.catchment.hydro_units.hydro_units[[('id', '-')]].copy()
        df[('connectivity', '-')] = [{} for _ in range(len(df))]

        # Loop over every hydro unit id and compute the flow accumulation
        flow_acc_tot = np.zeros_like(self.catchment.map_unit_ids)
        for unit_id in df[('id', '-')]:
            mask_unit = self.catchment.map_unit_ids == unit_id
            flow_acc = grid.accumulation(
                flow_dir,
                mask=mask_unit,
                routing='d8',
                nodata_out=np.float64(0)
            )
            flow_acc_np = flow_acc.view(np.ndarray)
            flow_acc_tot = np.maximum(flow_acc_tot, flow_acc_np)

        # Transform to numpy arrays
        flow_dir = flow_dir.view(np.ndarray)

        # Compute the connectivity
        self._sum_contributing_flow_acc(df, flow_acc_tot, flow_dir)

        def remove_connectivity_out(row: pd.Series) -> pd.Series:
            connectivity = row[('connectivity', '-')]
            if not connectivity:
                return row

            # Remove the key 0 if it exists
            if 0 in connectivity:
                del connectivity[0]

            return row

        def connect_to_neighbours(row: pd.Series) -> pd.Series:
            # Look for hydro units without connectivity and connect them to the neighboring hydro units
            unit_id = row[('id', '-')]
            connectivity = row[('connectivity', '-')]
            if connectivity:
                return row

            # Identify the neighboring hydro units
            mask_unit = self.catchment.map_unit_ids == unit_id
            dilated_mask = ndimage.binary_dilation(mask_unit, iterations=1)
            neighbor_ids = np.unique(self.catchment.map_unit_ids[dilated_mask])
            neighbor_ids = neighbor_ids[neighbor_ids != 0]
            neighbor_ids = neighbor_ids[neighbor_ids != unit_id]

            # Count the number of cells for each neighbor and set this value as the connectivity
            for neighbor_id in neighbor_ids:
                connectivity[int(neighbor_id)] = np.count_nonzero(
                    self.catchment.map_unit_ids[dilated_mask] == neighbor_id)

            row[('connectivity', '-')] = connectivity
            return row

        def _normalize_row(connect: dict) -> dict:
            total_flow = sum(connect.values())
            for k in connect:
                connect[k] /= total_flow

            return connect

        def _round_and_normalize_row(connect: dict, precision: int) -> dict:
            total = sum(connect.values())
            scaled = {k: v * (10 ** precision) for k, v in connect.items()}
            floored = {k: int(v) for k, v in scaled.items()}
            remainder = {k: v - floored[k] for k, v in scaled.items()}

            # Distribute the remaining difference to the largest remainders
            diff = int(round(total * (10 ** precision))) - sum(floored.values())
            for k in sorted(remainder, key=remainder.get, reverse=True)[:diff]:
                floored[k] += 1

            # Scale back to the original precision
            return {k: floored[k] / (10 ** precision) for k in floored}

        def normalize_connectivity(row: pd.Series, precision:int = 3) -> pd.Series:
            connectivity = row[('connectivity', '-')]
            if not connectivity:
                return row

            # If the maximum connectivity leaves the catchment, nullify the connectivity
            max_key = max(connectivity, key=connectivity.get)
            if max_key == 0 and not force_connectivity:
                row[('connectivity', '-')] = {}
                return row

            # Remove the key 0 if it exists
            if 0 in connectivity:
                del connectivity[0]

            # Normalize the connectivity within the catchment
            if sum(connectivity.values()) == 0:
                return row
            connectivity = _normalize_row(connectivity)

            # Remove connectivity if it is less than 0.01 (1% of the total flow)
            connectivity = {k: v for k, v in connectivity.items() if v >= 0.01}
            connectivity = _normalize_row(connectivity)

            # Round the connectivity values
            connectivity = _round_and_normalize_row(connectivity, precision)

            row[('connectivity', '-')] = connectivity
            return row

        def keep_highest_connectivity(row: pd.Series) -> pd.Series:
            connectivity = row[('connectivity', '-')]
            if not connectivity:
                return row

            # If the maximum connectivity leaves the catchment, nullify the connectivity
            max_key = max(connectivity, key=connectivity.get)
            if max_key == 0 and not force_connectivity:
                row[('connectivity', '-')] = {}
                return row

            # Keep only the highest connectivity
            connectivity = {max_key: 1.0}
            row[('connectivity', '-')] = connectivity
            return row

        if force_connectivity:
            df = df.apply(remove_connectivity_out, axis=1)
            df = df.apply(connect_to_neighbours, axis=1)

        if mode == 'multiple':
            df = df.apply(normalize_connectivity, axis=1, args=(precision,))
        elif mode == 'single':
            df = df.apply(keep_highest_connectivity, axis=1)
        else:
            raise ValueError("Unknown mode.")

        # Add the connectivity to the hydro units
        self.catchment.hydro_units.hydro_units[('connectivity', '-')] = df[
            ('connectivity', '-')]

        return df

    def _sum_contributing_flow_acc(
            self,
            df: pd.DataFrame,
            flow_acc: np.ndarray,
            flow_dir: np.ndarray
    ):
        # Loop over every cell in the flow accumulation grid
        for i in range(flow_acc.shape[0]):
            for j in range(flow_acc.shape[1]):
                # Get the unit id of the current cell
                unit_id = int(self.catchment.map_unit_ids[i, j])
                if unit_id == 0:
                    continue

                # Get the flow direction of the current cell
                flow_dir_cell = flow_dir[i, j]
                if flow_dir_cell <= 0:
                    continue

                # Get the unit id of the cell to which the current cell flows
                # Flow directions:
                # [N,  NE,  E, SE, S, SW, W, NW]
                # [64, 128, 1, 2,  4, 8, 16, 32]
                if flow_dir_cell == 1:
                    i_next, j_next = i, j + 1
                elif flow_dir_cell == 2:
                    i_next, j_next = i + 1, j + 1
                elif flow_dir_cell == 4:
                    i_next, j_next = i + 1, j
                elif flow_dir_cell == 8:
                    i_next, j_next = i + 1, j - 1
                elif flow_dir_cell == 16:
                    i_next, j_next = i, j - 1
                elif flow_dir_cell == 32:
                    i_next, j_next = i - 1, j - 1
                elif flow_dir_cell == 64:
                    i_next, j_next = i - 1, j
                elif flow_dir_cell == 128:
                    i_next, j_next = i - 1, j + 1
                else:
                    raise ValueError("Unknown flow direction.")

                if (i_next < 0 or i_next >= flow_acc.shape[0] or
                        j_next < 0 or j_next >= flow_acc.shape[1]):
                    continue

                unit_id_next = int(self.catchment.map_unit_ids[i_next, j_next])

                if unit_id_next == unit_id:
                    continue

                # If the current cell flows to a different unit, add a connection
                connect = df.loc[df[('id', '-')] == unit_id, ('connectivity', '-')]
                connect = connect.values[0]
                if unit_id_next not in connect:
                    connect[unit_id_next] = 0
                connect[unit_id_next] += float(flow_acc[i, j])
                df.loc[df[('id', '-')] == unit_id, ('connectivity', '-')] = [connect]
