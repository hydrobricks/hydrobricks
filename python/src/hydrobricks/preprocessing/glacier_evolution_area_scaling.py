from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

import numpy as np
import pandas as pd

import hydrobricks as hb
from hydrobricks.constants import ICE_WE

if TYPE_CHECKING:
    from hydrobricks.catchment import Catchment
    from hydrobricks.hydro_units import HydroUnits


class GlacierEvolutionAreaScaling:
    """
    Class for a simple volume-area scaling approach for glacier evolution. It computes
    the area per hydro unit (e.g. elevation band) for an incremental melt per band.
    It does not account for the glacier dynamic and should not be used for medium
    to large glaciers. The delta-h method is preferred for such cases.
    """

    def __init__(self):
        """
        Initialize the GlacierMassBalance class.
        """
        self.glacier_df = None
        self.hydro_units = None
        self.hydro_unit_ids = None
        self.catchment_area = None

        # Tables
        self.lookup_table_area = None
        self.lookup_table_volume = None
        self.we = None
        self.areas_pc = None

        # Pixel-wise ice water equivalent (we) for each hydro unit
        self.px_ice_we = None

    def compute_lookup_table(
            self,
            catchment: Catchment,
            ice_thickness: str | Path,
            nb_increments: int = 100
    ):
        """
        Extract the initial ice thickness to be used in compute_lookup_table()
        for the glacier mass balance calculation.

        Parameters
        ----------
        catchment
            The catchment object.
        ice_thickness
            Path to the TIF file containing the glacier thickness in meters.
        nb_increments
            Number of increments for glacier mass balance calculation. Default is 100.
        """

        # Check that the catchment has been discretized
        if catchment.map_unit_ids is None:
            raise ValueError("Catchment has not been discretized. "
                             "Please run create_elevation_bands() first.")

        # Extract the ice thickness from a TIF file.
        if not hb.has_pyproj:
            raise ImportError("pyproj is required to do this.")

        self.hydro_units = catchment.hydro_units.hydro_units
        self.catchment_area = np.sum(self.hydro_units.area.values)

        self._extract_ice_thickness(catchment, ice_thickness)
        self._initialization(nb_increments)

        # Compute the ice w.e. mass to remove per increment
        mass_change = self.we[0, :] * self.areas_pc[0, :] / nb_increments

        for increment in range(1, nb_increments):  # last row kept with zeros
            self._update_glacier_thickness(increment, mass_change)
            self._width_scaling(increment, catchment=catchment)

        self._update_lookup_tables()

    def get_lookup_table_area(self) -> pd.DataFrame:
        """
        Get the glacier area evolution lookup table.

        Returns
        -------
        The glacier area evolution lookup table.
        """
        return pd.DataFrame(
            self.lookup_table_area,
            index=range(self.lookup_table_area.shape[0]),
            columns=self.hydro_unit_ids
        )

    def get_lookup_table_volume(self) -> pd.DataFrame:
        """
        Get the glacier volume evolution lookup table.

        Returns
        -------
        The glacier volume evolution lookup table.
        """
        return pd.DataFrame(
            self.lookup_table_volume,
            index=range(self.lookup_table_volume.shape[0]),
            columns=self.hydro_unit_ids
        )

    def save_as_csv(self, output_dir: str | Path):
        """
        Save the glacier evolution lookup table as a CSV file.

        Parameters
        ----------
        output_dir
            Path to the directory where the CSV file will be saved.
        """
        output_dir = Path(output_dir)

        # Write record to the lookup table
        lookup_table_area = pd.DataFrame(
            self.lookup_table_area,
            index=range(self.lookup_table_area.shape[0]),
            columns=self.hydro_unit_ids
        )
        lookup_table_area.to_csv(
            output_dir / "glacier_evolution_lookup_table_area.csv",
            index=False
        )

        lookup_table_volume = pd.DataFrame(
            self.lookup_table_volume,
            index=range(self.lookup_table_volume.shape[0]),
            columns=self.hydro_unit_ids
        )
        lookup_table_volume.to_csv(
            output_dir / "glacier_evolution_lookup_table_volume.csv",
            index=False
        )

        if self.areas_pc is not None:
            columns = pd.MultiIndex.from_arrays(
                [range(len(self.areas_pc[0])),
                 self.hydro_unit_ids],
                names=['id',
                       'hydro_unit_id'])
            details_glacier_areas = pd.DataFrame(
                self.areas_pc * self.catchment_area,
                index=range(self.areas_pc.shape[0]),
                columns=columns)
            details_glacier_areas.to_csv(
                output_dir / "details_glacier_areas_evolution.csv",
                index=False
            )

        if self.we is not None:
            columns = pd.MultiIndex.from_arrays(
                [range(len(self.we[0])),
                 self.hydro_unit_ids],
                names=['id',
                       'hydro_unit_id']
            )
            details_glacier_we = pd.DataFrame(
                self.we,
                index=range(self.we.shape[0]),
                columns=columns)
            details_glacier_we.to_csv(
                output_dir / "details_glacier_we_evolution.csv",
                index=False
            )

    def _extract_ice_thickness(self, catchment, ice_thickness):
        catchment.extract_attribute_raster(
            ice_thickness,
            'ice_thickness',
            resample_to_dem_resolution=True,
            resampling='average'
        )
        ice_thickness = catchment.attributes['ice_thickness']['data']
        ice_thickness[catchment.dem_data == 0] = 0.0
        glaciers_mask = np.zeros(catchment.dem_data.shape)
        glaciers_mask[ice_thickness > 0] = 1

        glacier_patches = self._get_glacier_patches(
            catchment, glaciers_mask)

        # Create the dataframe for the glacier data
        glacier_df = None
        for unit_id, area in glacier_patches:
            new_row = pd.DataFrame(
                [[unit_id, area, 0.0]],
                columns=[('hydro_unit_id', '-'),
                         ('glacier_area', 'm2'),
                         ('glacier_thickness', 'm')]
            )
            if glacier_df is None:
                glacier_df = new_row
            else:
                glacier_df = pd.concat([glacier_df, new_row], ignore_index=True)

        self.px_ice_we = np.empty((1, len(glacier_df)), dtype=object)

        # Update the dataframe with the ice thickness
        for i_row, (idx, row) in enumerate(glacier_df.iterrows()):
            unit_id = row[('hydro_unit_id', '-')]

            # Get the ice thickness for the corresponding band and unit
            mask_unit = catchment.map_unit_ids == unit_id
            masked_thickness = ice_thickness[mask_unit]
            masked_thickness = masked_thickness[masked_thickness > 0]
            masked_thickness = masked_thickness[~np.isnan(masked_thickness)]

            self.px_ice_we[0, i_row] = masked_thickness * ICE_WE * 1000

            # Compute the mean thickness
            if masked_thickness.size > 0:
                mean_thickness = float(np.mean(masked_thickness))
            else:
                mean_thickness = 0.0

            glacier_df.at[idx, ('glacier_thickness', 'm')] = mean_thickness

        self.glacier_df = glacier_df

    def _initialization(self, nb_increments):
        # Add rows corresponding to the number of increments
        cols = self.px_ice_we.shape[1]
        new_rows = np.empty((nb_increments, cols), dtype=object)
        self.px_ice_we = np.vstack([self.px_ice_we, new_rows])

        # Extract the relevant columns
        initial_areas_m2 = self.glacier_df[('glacier_area', 'm2')].values
        initial_we_mm = self.glacier_df[('glacier_thickness', 'm')].values
        initial_we_mm *= ICE_WE * 1000
        hydro_unit_ids = self.glacier_df[('hydro_unit_id', '-')].values

        self.hydro_unit_ids = hydro_unit_ids
        self.we = np.zeros((nb_increments + 1, len(hydro_unit_ids)))
        self.we[0] = initial_we_mm
        self.areas_pc = np.zeros((nb_increments + 1, len(hydro_unit_ids)))
        self.areas_pc[0] = initial_areas_m2 / self.catchment_area
        self.lookup_table_area = np.zeros((nb_increments + 1, len(hydro_unit_ids)))
        self.lookup_table_volume = np.zeros((nb_increments + 1, len(hydro_unit_ids)))

    def _update_glacier_thickness(self, increment: int, mass_change: np.ndarray):
        # Copy the previous increment values as starting point
        self.we[increment, :] = self.we[increment - 1, :]
        self.px_ice_we[increment, :] = self.px_ice_we[increment - 1, :].copy()

        # Compute the melt
        for i_unit in np.arange(len(self.hydro_unit_ids)):
            change = mass_change[i_unit]
            melt = change / self.areas_pc[increment - 1, i_unit]

            # Update the glacier water equivalent of the pixels
            px_values = self.px_ice_we[increment, i_unit]
            px_values -= melt

            # Ensure that the pixel-wise ice water equivalent is not negative
            while np.any(px_values < 0):
                px_melt_deficit = - np.sum(px_values[px_values < 0])
                px_melt = px_melt_deficit / len(px_values[px_values > 0])
                px_values = np.where(px_values <= 0, 0, px_values - px_melt)

            self.px_ice_we[increment, i_unit] = px_values

            # Update the glacier water equivalent for the hydro unit
            new_we = np.mean(px_values)
            ref_we = self.we[increment, i_unit]
            assert np.isclose(new_we, ref_we - melt, atol=1e-02), \
                (f"Mismatch in glacier w.e. for unit {i_unit} "
                 f"({ref_we - melt} != {new_we})")
            self.we[increment, i_unit] = new_we

    def _width_scaling(self, increment: int, catchment: Catchment):
        self.areas_pc[increment] = self.areas_pc[increment - 1]
        px_area = catchment.get_dem_pixel_area()

        # Update the glacier area based on the pixel-wise ice water equivalent
        areas = np.zeros(len(self.hydro_unit_ids))
        for i in range(len(self.hydro_unit_ids)):
            ice_we = self.px_ice_we[increment, i]
            areas[i] = np.count_nonzero(ice_we) * px_area
            # Drop pixels with no ice water equivalent
            self.px_ice_we[increment, i] = ice_we[ice_we > 0]

        self.areas_pc[increment, :] = areas / self.catchment_area

        # Adjust the glacier ice w.e. per part (px with 0 ice w.e. were dropped)
        self.we[increment] = np.array([
            np.mean(arr) if arr.size > 0 else 0
            for arr in self.px_ice_we[increment]
        ])

    def _update_lookup_tables(self):
        """
        Step 5 (6) of Seibert et al. (2018): "Sum the total (width-scaled) areas for all
        respective elevation bands which are covered by glaciers (i.e. glacier water
        equivalent ≥ 0) for each elevation zone."
        """
        # Update elevation zone areas
        for i, hydro_unit_id in enumerate(np.unique(self.hydro_unit_ids)):
            indices = np.where(self.hydro_unit_ids == hydro_unit_id)[0]
            if len(indices) != 0:
                areas = self.areas_pc[:, indices] * self.catchment_area
                self.lookup_table_area[:, i] = np.sum(areas, axis=1)
                volumes = self.we[:, indices] * areas / (ICE_WE * 1000)
                self.lookup_table_volume[:, i] = np.sum(volumes, axis=1)
            else:
                self.lookup_table_area[:, i] = 0
                self.lookup_table_volume[:, i] = 0

    @staticmethod
    def _get_glacier_patches(
            catchment: Catchment,
            glaciers_mask: np.ndarray
    ) -> list[tuple[int, int, float]]:
        # Extract the pixel size
        px_area = catchment.get_dem_pixel_area()

        glacier_patches = []

        # Get the hydro unit ids for the glacier mask
        unit_ids = np.unique(catchment.map_unit_ids[glaciers_mask > 0])
        unit_ids = unit_ids[unit_ids != 0]

        for unit_id in unit_ids:
            mask_unit_id = catchment.map_unit_ids[glaciers_mask > 0] == unit_id
            area = np.count_nonzero(mask_unit_id) * px_area
            glacier_patches.append((unit_id, area))

        return glacier_patches