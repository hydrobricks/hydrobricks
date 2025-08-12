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

if hb.has_shapely:
    from shapely.geometry import MultiPolygon, mapping

if hb.has_rasterio:
    from rasterio.mask import mask


class GlacierEvolutionDeltaH:
    """
    Class for glacier mass balance. It is based on the following papers:
    - Seibert, J., Vis, M. J. P., Kohn, I., Weiler, M., and Stahl, K.: Technical note:
      Representing glacier geometry changes in a semi-distributed hydrological model,
      Hydrol. Earth Syst. Sci., 22, 2211–2224,
      https://doi.org/10.5194/hess-22-2211-2018, 2018.
    - Huss, M., Jouvet, G., Farinotti, D., and Bauder, A.: Future high-mountain
      hydrology: a new parameterization of glacier retreat, Hydrol. Earth Syst. Sci.,
      14, 815–829, https://doi.org/10.5194/hess-14-815-2010, 2010.
    """

    def __init__(self, hydro_units: HydroUnits | None = None):
        """
        Initialize the GlacierMassBalance class.

        Parameters
        ----------
        hydro_units
            The hydro unit object.
        """
        self.glacier_df = None
        self.hydro_units = None
        self.catchment_area = None
        if hydro_units is not None:
            self.hydro_units = hydro_units.hydro_units
            self.catchment_area = np.sum(self.hydro_units.area.values)
        self.hydro_unit_ids = None
        self.elev_bands = None  # Pure elevation bands for glacier discretization.
        self.elev_bands_parts = None  # Elevation bands subdivided by hydro units.
        self.elev_bands_indices = None  # Indices for the elevation bands parts.
        self.pixel_based_approach = False
        self.sub_elevation_parts = False

        # Tables
        self.lookup_table_area = None
        self.lookup_table_volume = None
        self.we_parts = None  # Glacier water equivalent (we) per part.
        self.we_bands = None  # Glacier water equivalent (we) per elevation band.
        self.areas_pc_parts = None  # Glacier areas as a percentage of the catch. area.
        self.areas_pc_bands = None  # Same per elevation band.

        # Delta-h parametrization based on Huss et al. (2010).
        self.a_coeff = np.nan
        self.b_coeff = np.nan
        self.c_coeff = np.nan
        self.gamma_coeff = np.nan

        # Method-related attributes
        self.initial_total_glacier_we = np.nan
        self.norm_elevations_bands = np.nan
        self.norm_delta_we_bands = np.nan
        self.scaling_factor_mm = np.nan
        self.excess_melt_we = 0

        # Pixel-wise ice water equivalent (we) for each elevation band
        self.px_ice_we = None

    def compute_initial_ice_thickness(
            self,
            catchment: Catchment,
            glacier_outline: str | None = None,
            ice_thickness: str | None = None,
            elevation_bands_distance: int = 10,
            pixel_based_approach: bool = True
    ) -> pd.DataFrame:
        """
        Extract the initial ice thickness to be used in compute_lookup_table()
        for the glacier mass balance calculation.

        Parameters
        ----------
        catchment
            The catchment object.
        glacier_outline
            Path to the SHP file containing the glacier extents.
            If None, the glacier extents are extracted from the ice thickness geotiff.
            Either this or ice_thickness should be provided.
        ice_thickness
            Path to the TIF file containing the glacier thickness in meters.
            If None, the ice thickness is estimated based on the glacier area
            using the Bahr et al. (1997) formula.
            Either this or glacier_outline should be provided.
        elevation_bands_distance
            Distance between elevation bands in meters. Default is 10 m.
        pixel_based_approach
            Whether to compute the glacier area evolution from the topography.
            If True, the glacier area is computed from the topography and the
            ice thicknesses are updated accordingly. Otherwise, the glacier area
            is updated based on the Bahr et al. (1997) formula.
            Default is True.

        Returns
        -------
        The glacier_df DataFrame containing the glacier data.
        """
        if glacier_outline is None and ice_thickness is None:
            raise ValueError("Either glacier_outline or ice_thickness "
                             "should be provided.")

        if glacier_outline is not None and ice_thickness is not None:
            raise ValueError("Either glacier_outline or ice_thickness "
                             "should be provided, not both.")

        self.pixel_based_approach = pixel_based_approach

        # Discretize the DEM into elevation bands at the given distance
        elevations, map_bands_ids = self._discretize_elevation_bands(
            catchment,
            elevation_bands_distance
        )

        # Extract the ice thickness from a TIF file created either from geophysical
        # measurements or calculated based on an inversion of surface topography
        # (Farinotti et al., 2009a,b; Huss et al., 2010)).
        if ice_thickness is not None:
            if not hb.has_pyproj:
                raise ImportError("pyproj is required to do this.")

            # Extract the ice thickness and resample it to the DEM resolution
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
        else:
            # Extract the glacier cover from the shapefile
            glaciers_mask = self._extract_glacier_mask_from_shapefile(
                catchment, glacier_outline)

        glacier_patches = self._get_glacier_patches(
            catchment, map_bands_ids, glaciers_mask)

        # Create the dataframe for the glacier data
        glacier_df = None
        for band_id, unit_id, area in glacier_patches:
            new_row = pd.DataFrame(
                [[band_id, elevations[band_id - 1], area, 0.0, unit_id]],
                columns=[('band_id', '-'),
                         ('elevation', 'm'),
                         ('glacier_area', 'm2'),
                         ('glacier_thickness', 'm'),
                         ('hydro_unit_id', '-')]
            )
            if glacier_df is None:
                glacier_df = new_row
            else:
                glacier_df = pd.concat([glacier_df, new_row], ignore_index=True)

        # Extract the ice thickness from a TIF file created either from geophysical
        # measurements or calculated based on an inversion of surface topography
        # (Farinotti et al., 2009a,b; Huss et al., 2010)).
        if ice_thickness is not None:
            if self.pixel_based_approach:
                self.px_ice_we = np.empty((1, len(glacier_df)), dtype=object)

            # Update the dataframe with the ice thickness
            for i_row, (idx, row) in enumerate(glacier_df.iterrows()):
                band_id = row[('band_id', '-')]
                unit_id = row[('hydro_unit_id', '-')]

                # Get the ice thickness for the corresponding band and unit
                mask_band = map_bands_ids == band_id
                mask_unit = catchment.map_unit_ids == unit_id
                masked_thickness = ice_thickness[mask_band & mask_unit]
                masked_thickness = masked_thickness[masked_thickness > 0]
                masked_thickness = masked_thickness[~np.isnan(masked_thickness)]

                if self.pixel_based_approach:
                    self.px_ice_we[0, i_row] = masked_thickness * ICE_WE * 1000

                # Compute the mean thickness
                if masked_thickness.size > 0:
                    mean_thickness = float(np.mean(masked_thickness))
                else:
                    mean_thickness = 0.0

                glacier_df.at[idx, ('glacier_thickness', 'm')] = mean_thickness

        else:
            # Estimation of the overall ice volume using the Bahr et al. (1997)
            # formula, to estimate the mean ice thickness.
            total_area = np.sum(glacier_df[('glacier_area', 'm2')])
            total_volume = np.power(total_area, 1.36)
            mean_thickness = total_volume / total_area
            nonzero_mask = glacier_df[('glacier_area', 'm2')] != 0
            glacier_df[('glacier_thickness', 'm')] = glacier_df[('glacier_area', 'm2')]
            glacier_df.loc[nonzero_mask, ('glacier_thickness', 'm')] = mean_thickness

        self.glacier_df = glacier_df
        self.hydro_units = catchment.hydro_units.hydro_units
        self.catchment_area = np.sum(self.hydro_units.area.values)

        return self.glacier_df

    def compute_lookup_table(
            self,
            catchment: Catchment | None = None,
            glacier_profile_csv: str | None = None,
            glacier_df: pd.DataFrame | None = None,
            nb_increments: int = 100,
            update_width: bool = True,
            update_width_reference: str = 'initial'
    ):
        """
        Prepare the glacier mass balance lookup table. The glacier mass balance is
        calculated using the delta-h method (Huss et al., 2010) using the
        lookup table method (Seibert et al., 2018). The glacier mass balance is
        calculated for each elevation band and melt increment and is synthesized
        in a lookup table per hydro unit.

        Parameters
        ----------
        catchment
            The catchment object. Needed when pixel_based_approach is True.
        glacier_profile_csv
            Path to the CSV file containing glacier data. An elevation band is smaller
            than a hydro unit, usually around 10 m high. It should contain the
            following columns:
            - elevation: Elevation (m) for each elevation band i (e.g. every 10m).
            - glacier_area: Glacier area (m2) for each elevation band i.
            - glacier_thickness: Glacier thickness (m) for each elevation band i.
            - hydro_unit_id: Hydro unit ID for each elevation band i.

            Format example:
            elevation   glacier_area    glacier_thickness   hydro_unit_id
            m           m2              m                   -
            1670        0               0                   2
            1680        0               0                   2
            1690        2500            14.7                2
            1700        9375            23.2                3
            1710        11875           25.2                3
            1720        9375            23.2                3
            1730        9375            23.2                3
            ...         ...             ...                 ...

            If not provided, the glacier data is assumed to be already loaded in the
            glacier_df attribute or to be passed as a DataFrame.

        glacier_df
            DataFrame containing glacier data. Alternative to glacier_profile_csv.
        nb_increments
            Number of increments for glacier mass balance calculation. Default is 100.
        update_width
            Whether to update the glacier width at each iteration (Eq. 7 Seibert et al.,
            2018). Default is True. Ignored if pixel_based_approach is True.
        update_width_reference
            Reference for updating the glacier width (Eq. 7 Seibert et al., 2018).
            Default is 'initial'. Options are:
            - 'initial': Use the initial glacier width.
            - 'previous': Use the glacier width from the previous iteration.
            Ignored if pixel_based_approach is True.
        """
        assert self.hydro_units is not None, \
            "Hydro units are not defined. Please load them first."
        assert self.catchment_area is not None, \
            "Catchment area is not defined."

        if glacier_profile_csv is not None and glacier_df is not None:
            raise ValueError("Please provide either glacier_profile_csv or glacier_df, "
                             "not both.")

        if glacier_profile_csv is not None:
            # Read the glacier data
            self.glacier_df = pd.read_csv(glacier_profile_csv, header=[0, 1])
        elif glacier_df is not None:
            self.glacier_df = glacier_df
        assert self.glacier_df is not None, \
            "Glacier data is not defined. Please provide a CSV file or a DataFrame."

        if self.pixel_based_approach and catchment is None:
            raise ValueError("When pixel_based_approach is True, "
                             "the catchment object must be provided.")

        if self.pixel_based_approach:
            # Add rows corresponding to the number of increments
            cols = self.px_ice_we.shape[1]
            new_rows = np.empty((nb_increments, cols), dtype=object)
            self.px_ice_we = np.vstack([self.px_ice_we, new_rows])

        # We have to remove the bands with no glacier area
        # (otherwise the min and max elevations are wrong)
        self.glacier_df = self.glacier_df.drop(
            self.glacier_df[self.glacier_df[('glacier_area', 'm2')] == 0].index)

        # Extract the relevant columns
        elev_bands_parts = self.glacier_df[('elevation', 'm')].values
        initial_areas_m2 = self.glacier_df[('glacier_area', 'm2')].values
        initial_we_mm = self.glacier_df[('glacier_thickness', 'm')].values
        initial_we_mm *= ICE_WE * 1000
        hydro_unit_ids = self.glacier_df[('hydro_unit_id', '-')].values

        self.excess_melt_we = 0
        self.elev_bands_parts = elev_bands_parts
        self.hydro_unit_ids = hydro_unit_ids
        self.we_parts = np.zeros((nb_increments + 1, len(elev_bands_parts)))
        self.we_parts[0] = initial_we_mm
        self.areas_pc_parts = np.zeros((nb_increments + 1, len(elev_bands_parts)))
        self.areas_pc_parts[0] = initial_areas_m2 / self.catchment_area
        self.lookup_table_area = np.zeros(
            (nb_increments + 1, len(np.unique(hydro_unit_ids)))
        )
        self.lookup_table_volume = np.zeros(
            (nb_increments + 1, len(np.unique(hydro_unit_ids)))
        )

        # In case the catchment is discretized by something else than elevation only
        self.elev_bands, self.elev_bands_indices = np.unique(
            elev_bands_parts,
            return_inverse=True
        )

        if len(self.elev_bands) == len(elev_bands_parts):
            self.sub_elevation_parts = False
        else:
            self.sub_elevation_parts = True

        self._init_parts_containers(nb_increments)
        self._initialization()

        for increment in range(1, nb_increments):  # last row kept with zeros
            self._compute_delta_h(increment, nb_increments)
            self._update_glacier_thickness(increment)
            self._width_scaling(
                increment,
                update_width,
                update_width_reference,
                catchment=catchment
            )

        if not update_width and not self.pixel_based_approach:
            self._final_width_scaling()

        self._compute_sub_band_parts()
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
            columns=np.unique(self.hydro_unit_ids)
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
            columns=np.unique(self.hydro_unit_ids)
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
            columns=np.unique(self.hydro_unit_ids)
        )
        lookup_table_area.to_csv(
            output_dir / "glacier_evolution_lookup_table_area.csv",
            index=False
        )

        lookup_table_volume = pd.DataFrame(
            self.lookup_table_volume,
            index=range(self.lookup_table_volume.shape[0]),
            columns=np.unique(self.hydro_unit_ids)
        )
        lookup_table_volume.to_csv(
            output_dir / "glacier_evolution_lookup_table_volume.csv",
            index=False
        )

        if self.areas_pc_parts is not None:
            columns = pd.MultiIndex.from_arrays(
                [range(len(self.areas_pc_parts[0])),
                 self.hydro_unit_ids,
                 self.elev_bands_parts],
                names=['id',
                       'hydro_unit_id',
                       'elevation_band'])
            details_glacier_areas = pd.DataFrame(
                self.areas_pc_parts * self.catchment_area,
                index=range(self.areas_pc_parts.shape[0]),
                columns=columns)
            details_glacier_areas.to_csv(
                output_dir / "details_glacier_areas_evolution.csv",
                index=False
            )

        if self.we_parts is not None:
            columns = pd.MultiIndex.from_arrays(
                [range(len(self.we_parts[0])),
                 self.hydro_unit_ids,
                 self.elev_bands_parts],
                names=['id',
                       'hydro_unit_id',
                       'elevation_band']
            )
            details_glacier_we = pd.DataFrame(
                self.we_parts,
                index=range(self.we_parts.shape[0]),
                columns=columns)
            details_glacier_we.to_csv(
                output_dir / "details_glacier_we_evolution.csv",
                index=False
            )

    def _init_parts_containers(self, nb_increments: int):
        """
        Initialize the containers for sub-elevation parts.
        """
        if not self.sub_elevation_parts:
            self.areas_pc_bands = self.areas_pc_parts
            self.we_bands = self.we_parts
            self.elev_bands_indices = np.arange(len(self.elev_bands))
        else:
            # Loop over elevation groups and sum relevant columns
            self.areas_pc_bands = np.zeros((nb_increments + 1, len(self.elev_bands)))
            self.we_bands = np.zeros((nb_increments + 1, len(self.elev_bands)))
            for _, elev_idx in enumerate(np.arange(len(self.elev_bands))):
                mask = self.elev_bands_indices == elev_idx
                self.areas_pc_bands[0, elev_idx] = self.areas_pc_parts[0, mask].sum()
                pc = (self.areas_pc_parts[0, mask] /
                      self.areas_pc_bands[0, elev_idx])
                self.we_bands[0, elev_idx] = np.sum(self.we_parts[0, mask] * pc)

    def _initialization(self):
        """
        Step 1 of Seibert et al. (2018): Calculation of the initial total glacier mass
        and normalization of glacier elevations. Selection of the right glacial
        parametrization (delta-h method).
        """
        # Calculate total glacier mass (Eq. 2)
        # areas: area (expressed as a proportion of the catchment area) for each
        # elevation band i
        self.initial_total_glacier_we = np.sum(self.areas_pc_parts[0] *
                                               self.we_parts[0, :])

        # Normalize glacier elevations (Eq. 3)
        # elevations: absolute elevation for each elevation band i
        max_elevation_m = np.max(self.elev_bands_parts)
        min_elevation_m = np.min(self.elev_bands_parts)
        self.norm_elevations_bands = ((max_elevation_m - self.elev_bands) /
                                      (max_elevation_m - min_elevation_m))

        # Choose the glacial parametrization (Huss et al., 2010, Fig. 2a)
        glacier_area_km2 = (np.sum(self.areas_pc_parts[0]) *
                            self.catchment_area / (1000 * 1000))
        self._set_delta_h_parametrization(glacier_area_km2)

    def _set_delta_h_parametrization(self, glacier_area_km2: float):
        """
        Delta-h parametrization of the glacier. See Huss et al. (2010).
        """
        if glacier_area_km2 > 20:  # Large valley glacier
            self.a_coeff = -0.02
            self.b_coeff = 0.12
            self.c_coeff = 0.00
            self.gamma_coeff = 6
        elif 5 <= glacier_area_km2 <= 20:  # Medium valley glacier
            self.a_coeff = -0.05
            self.b_coeff = 0.19
            self.c_coeff = 0.01
            self.gamma_coeff = 4
        elif 0 < glacier_area_km2 < 5:  # Small glacier
            self.a_coeff = -0.30
            self.b_coeff = 0.60
            self.c_coeff = 0.09
            self.gamma_coeff = 2

    def _compute_delta_h(self, increment: int, nb_increments: int):
        """
        Step 2 of Seibert et al. (2018): Apply the delta-h method to calculate the
        change in glacier mass balance.
        """
        # Apply ∆h-parameterization (Eq. 4)
        # Gives the normalized (dimensionless) ice thickness change for each
        # elevation band i
        self.norm_delta_we_bands = (
                np.power(self.norm_elevations_bands + self.a_coeff, self.gamma_coeff) +
                self.b_coeff * (self.norm_elevations_bands + self.a_coeff) +
                self.c_coeff)

        # Calculate the scaling factor fs (Eq. 5)
        # The glacier volume change increment, added with the excess melt that could
        # not be melted in the previous step
        glacier_we_change_mm = (self.initial_total_glacier_we / nb_increments +
                                self.excess_melt_we)
        # "A scaling factor fS (mm), which scales the dimensionless ∆h, is computed
        # based on the glacier volume change M and on the area and normalized water
        # equivalent change for each of the elevation bands."
        self.scaling_factor_mm = glacier_we_change_mm / (
            np.sum(self.areas_pc_bands[increment - 1]
                   * self.norm_delta_we_bands))

    def _update_glacier_thickness(
            self,
            increment: int
    ):
        """
        Step 3 of Seibert et al. (2018): "For each elevation band reduce the glacier
        water equivalent according to the empirical functions from Huss et al. (2010)
        (Eq. 4) to compute the glacier geometry for the reduced mass (see Eq. 6).
        If the computed thickness change is larger than the remaining glacier thickness
        (most likely to occur at the glacier tongue [...]), the glacier thickness is
        reduced to zero, resulting in a glacier-free elevation band, and the portion of
        the glacier thickness change that would have resulted in a negative glacier
        thickness is included in the next iteration step (i.e. the next 1% melt)."
        """
        # Update glacier thicknesses (Eq. 6)
        if not self.pixel_based_approach:
            # Glacier water equivalent reduction computed per elevation band
            we_reduction_bands = self.scaling_factor_mm * self.norm_delta_we_bands

            # Update glacier thicknesses per elevation band
            new_we_bands = self.we_bands[increment - 1] - we_reduction_bands
            self.we_bands[increment] = np.where(new_we_bands < 0, 0, new_we_bands)

            # This excess melt is taken into account in Step 2.
            self.excess_melt_we = - np.sum(np.where(new_we_bands < 0, new_we_bands, 0) *
                                           self.areas_pc_bands[increment - 1])
            return

        # If the glacier area evolution is based on the topography
        # Glacier water equivalent reduction
        we_reduction_mm = self.scaling_factor_mm * self.norm_delta_we_bands

        # Copy the previous increment values as starting point
        self.we_bands[increment, :] = self.we_bands[increment - 1, :]
        self.we_parts[increment, :] = self.we_parts[increment - 1, :]
        self.px_ice_we[increment, :] = self.px_ice_we[increment - 1, :].copy()

        # Compute the melt
        excess_melt = np.zeros(len(self.elev_bands))
        for i_band in np.arange(len(self.elev_bands)):
            band_mask = self.elev_bands_indices == i_band
            melt = we_reduction_mm[i_band]

            # If the whole band melts, store the excess melt
            if self.we_bands[increment, i_band] < melt:
                excess_melt[i_band] = (self.we_bands[increment, i_band] - melt)
                self.we_bands[increment, i_band] = 0
                self.we_parts[increment, band_mask] = 0
                for idx in np.where(band_mask)[0]:
                    self.px_ice_we[increment, idx] = np.array([])
                continue

            # Otherwise, update the glacier water equivalent of the parts
            band_mask_idx = np.where(band_mask)[0]

            while melt > 0:
                excess_melt_parts = np.zeros(len(band_mask_idx))
                for i_part, idx in enumerate(band_mask_idx):
                    # Skip parts with no glacier w.e. (happens during iterations)
                    if self.we_parts[increment, idx] <= 0:
                        continue

                    # If the whole part melts, store the excess melt locally
                    if self.we_parts[increment, idx] < melt:
                        excess_melt_parts[i_part] = (
                                self.we_parts[increment, idx] - melt
                        )
                        self.we_parts[increment, idx] = 0
                        self.px_ice_we[increment, idx] = np.array([])
                        continue

                    # Otherwise, update the glacier water equivalent of the pixels
                    px_values = self.px_ice_we[increment, idx]
                    px_values -= melt

                    # Ensure that the pixel-wise ice water equivalent is not negative
                    while np.any(px_values < 0):
                        px_melt_deficit = - np.sum(px_values[px_values < 0])
                        px_melt = px_melt_deficit / len(px_values[px_values > 0])
                        px_values = np.where(px_values <= 0, 0, px_values - px_melt)

                    self.px_ice_we[increment, idx] = px_values

                    # Update the glacier water equivalent for the part
                    new_we = np.mean(px_values)
                    ref_we = self.we_parts[increment, idx]
                    assert np.isclose(new_we, ref_we - melt, atol=1e-02), \
                        (f"Mismatch in glacier w.e. for part {idx} "
                         f"({ref_we - melt} != {new_we})")
                    assert new_we >= 0, f"Negative glacier w.e. for part {idx}"
                    self.we_parts[increment, idx] = new_we

                # Compute the excess melt for the band
                excess_melt_parts_tot = - np.sum(
                    excess_melt_parts *
                    self.areas_pc_parts[increment - 1, band_mask]
                )

                # Scale the excess melt to area of the positive parts
                pos_areas = self.areas_pc_parts[increment - 1, band_mask]
                pos_areas = pos_areas[self.we_parts[increment, band_mask] > 0]
                melt = excess_melt_parts_tot / np.sum(pos_areas)

        # This excess melt is taken into account in Step 2.
        self.excess_melt_we = - np.sum(excess_melt * self.areas_pc_bands[increment - 1])

    def _width_scaling(
            self,
            increment: int,
            update_width: bool,
            update_width_reference: str = 'initial',
            catchment: Catchment = None,
    ):
        """
        Step 4 of Seibert et al. (2018): "The width scaling within each elevation band
        relates a decrease in glacier thickness to a reduction of the glacier area
        within the respective elevation band. In other words, this approach also allows
        for glacier area shrinkage at higher elevations, which mimics the typical
        spatial effect of the downwasting of glaciers. Relation from Bahr et al. (1997)"
        """
        if not self.pixel_based_approach:
            if update_width:
                if update_width_reference == 'previous':
                    pos_we = self.we_bands[increment - 1] > 0
                    self.areas_pc_bands[increment, pos_we] = (
                            self.areas_pc_bands[increment - 1, pos_we]
                            * np.power(self.we_bands[increment, pos_we] /
                                       self.we_bands[increment - 1, pos_we], 0.5))

                elif update_width_reference == 'initial':
                    pos_we = self.we_bands[0] > 0
                    self.areas_pc_bands[increment, pos_we] = (
                            self.areas_pc_bands[0, pos_we]
                            * np.power(self.we_bands[increment, pos_we] /
                                       self.we_bands[0, pos_we], 0.5))

                else:
                    raise ValueError("update_width_reference should be either "
                                     "'previous' or 'initial'.")

                # Conservation of the water equivalent
                pos_we = self.we_bands[increment] > 0
                self.we_bands[increment, pos_we] *= (
                        self.areas_pc_bands[increment - 1, pos_we] /
                        self.areas_pc_bands[increment, pos_we]
                )

                # Nullify the areas of the elevation bands with no glacier w.e.
                self.areas_pc_bands[increment, self.we_bands[increment] == 0] = 0

            else:
                # If the glacier width is not updated, keep the previous glacier area.
                self.areas_pc_bands[increment] = self.areas_pc_bands[increment - 1]

                # Nullify the areas of the elevation bands with no glacier water equiv.
                self.areas_pc_bands[increment, self.we_bands[increment] == 0] = 0

        else:
            self.areas_pc_bands[increment] = self.areas_pc_bands[increment - 1]
            px_area = catchment.get_dem_pixel_area()

            # Update the glacier area based on the pixel-wise ice water equivalent
            areas = np.zeros(len(self.elev_bands_parts))
            for i in range(len(self.elev_bands_parts)):
                ice_we = self.px_ice_we[increment, i]
                areas[i] = np.count_nonzero(ice_we) * px_area
                # Drop pixels with no ice water equivalent
                self.px_ice_we[increment, i] = ice_we[ice_we > 0]

            self.areas_pc_parts[increment, :] = areas / self.catchment_area

            # Adjust the glacier ice w.e. per part (px with 0 ice w.e. were dropped)
            self.we_parts[increment] = np.array([
                np.mean(arr) if arr.size > 0 else 0
                for arr in self.px_ice_we[increment]
            ])

            # Updating the band areas and band thicknesses
            for elev_idx in np.arange(len(self.elev_bands)):
                band_mask = self.elev_bands_indices == elev_idx
                self.areas_pc_bands[increment, elev_idx] = self.areas_pc_parts[
                    increment, band_mask].sum()
                band_we = np.concatenate(self.px_ice_we[increment, band_mask])
                if band_we.size == 0:
                    self.we_bands[increment, elev_idx] = 0
                else:
                    self.we_bands[increment, elev_idx] = np.mean(band_we)

    def _final_width_scaling(self):
        """
        Similar to _width_scaling, but for the case when the glacier area is not
        updated during the loop.
        """
        # Width scaling (Eq. 7)
        for i in range(1, len(self.we_bands)):
            pos_we = self.we_bands[0] > 0
            self.areas_pc_bands[i, pos_we] = (
                    self.areas_pc_bands[0, pos_we] *
                    np.power(self.we_bands[i, pos_we] /
                             self.we_bands[0, pos_we],
                             0.5)
            )

            # Conservation of the water equivalent
            pos_we = self.we_bands[i] > 0
            self.we_bands[i, pos_we] *= (
                    self.areas_pc_bands[0, pos_we] /
                    self.areas_pc_bands[i, pos_we])

    def _adjust_areas_parts(self, increment):
        """
        Adjust the areas of the sub-band part.
        """
        self.areas_pc_parts[increment] = self.areas_pc_parts[0]

        # Nullify the areas of the elevation bands with no glacier water equivalent
        self.areas_pc_parts[increment, self.we_parts[increment] == 0] = 0

        for _, elev_idx in enumerate(np.arange(len(self.elev_bands))):
            band_mask = ((self.elev_bands_indices == elev_idx) &
                         (self.we_parts[increment] > 0))

            if not np.any(band_mask):
                continue

            # Scaling each part of the total band proportionally
            self.areas_pc_parts[increment, band_mask] *= (
                    self.areas_pc_bands[increment, elev_idx] /
                    self.areas_pc_bands[0, elev_idx])

            # No need to adjust the glacier thicknesses here, as they are already
            # accounted for by the scaled band w.e.

    def _adjust_we_parts(self, increment):
        """
        Adjust the volume of the sub-band part.
        """
        # Decreasing glacier w.e. per elevation band parts proportionally
        # to the glacier area per elevation band parts
        self.we_parts[increment] = self.we_parts[0]
        for elev_idx in range(len(self.elev_bands)):
            band_mask = self.elev_bands_indices == elev_idx
            if self.we_bands[increment, elev_idx] == 0:
                self.we_parts[increment, band_mask] = 0
                continue
            self.we_parts[increment, band_mask] *= (
                    self.we_bands[increment, elev_idx] /
                    self.we_bands[0, elev_idx]
            )

    def _compute_sub_band_parts(self):
        """
        For discretization of hydro units into smaller units than elevation bands,
        we need to compute the glacier areas and water equivalent for each part.
        """
        if self.pixel_based_approach:
            return

        if not self.sub_elevation_parts:
            self.areas_pc_parts = self.areas_pc_bands
            self.we_parts = self.we_bands
            return

        for i in range(1, len(self.we_bands)):
            # Update the w.e. per elevation band parts
            self._adjust_we_parts(i)

            # Redistribution following the sub-elevation discretization.
            self._adjust_areas_parts(i)

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
                areas = self.areas_pc_parts[:, indices] * self.catchment_area
                self.lookup_table_area[:, i] = np.sum(areas, axis=1)
                volumes = self.we_parts[:, indices] * areas / (ICE_WE * 1000)
                self.lookup_table_volume[:, i] = np.sum(volumes, axis=1)
            else:
                self.lookup_table_area[:, i] = 0
                self.lookup_table_volume[:, i] = 0

    @staticmethod
    def _discretize_elevation_bands(
            catchment: Catchment,
            elevation_bands_distance: int = 10
    ) -> tuple[np.ndarray, np.ndarray]:
        """ Discretize the DEM into elevation bands at the given distance."""
        # Check that the catchment has been discretized
        if catchment.map_unit_ids is None:
            raise ValueError("Catchment has not been discretized. "
                             "Please run create_elevation_bands() first.")

        hydro_units = catchment.hydro_units.hydro_units

        # Check that the catchment hydro units are consistent with the desired
        # elevation_bands_distance parameter
        elevations = np.unique(np.sort(hydro_units['elevation'].values.ravel()))
        steps = np.diff(elevations)
        hu_steps = np.min(steps)
        if hu_steps % elevation_bands_distance != 0:
            raise ValueError(f"Hydro unit elevation range ({hu_steps}) must be a "
                             f"multiple of the elevation bands distance "
                             f"({elevation_bands_distance}). Please adjust the "
                             f"elevation_bands_distance parameter.")

        # Discretize the DEM into elevation bands at the given distance
        min_elevation = hydro_units['elevation'].min().values[0] - hu_steps / 2
        max_elevation = hydro_units['elevation'].max().values[0] + hu_steps / 2
        elevations = np.arange(
            min_elevation,
            max_elevation + elevation_bands_distance,
            elevation_bands_distance
        )

        map_bands_ids = np.zeros(catchment.dem_data.shape)
        for i in range(len(elevations) - 1):
            val_min = elevations[i]
            val_max = elevations[i + 1]
            mask_band = np.logical_and(
                catchment.dem_data >= val_min,
                catchment.dem_data < val_max
            )
            map_bands_ids[mask_band] = i + 1

        map_bands_ids = map_bands_ids.astype(hb.rasterio.uint16)

        # Set the elevation band values to the middle of the band
        elevations = elevations + elevation_bands_distance / 2

        return elevations, map_bands_ids

    def _extract_glacier_mask_from_shapefile(
            self,
            catchment: Catchment,
            glacier_outline: str | Path
    ) -> np.ndarray:
        """ Extract the glacier cover from shapefiles."""
        # Clip the glaciers to the catchment extent
        all_glaciers = hb.gpd.read_file(glacier_outline)
        all_glaciers.to_crs(catchment.crs, inplace=True)
        if catchment.outline[0].geom_type == 'MultiPolygon':
            glaciers = hb.gpd.clip(all_glaciers, catchment.outline[0])
        elif catchment.outline[0].geom_type == 'Polygon':
            glaciers = hb.gpd.clip(all_glaciers, MultiPolygon(catchment.outline))
        else:
            raise ValueError("The catchment outline must be a (multi)polygon.")
        glaciers = self._simplify_df_geometries(glaciers)

        # Get the glacier mask
        glaciers_mask = self._mask_dem(catchment, glaciers, -9999)

        return glaciers_mask

    @staticmethod
    def _get_glacier_patches(
            catchment: Catchment,
            map_bands_ids: np.ndarray,
            glaciers_mask: np.ndarray
    ) -> list[tuple[int, int, float]]:
        # Extract the pixel size
        px_area = catchment.get_dem_pixel_area()

        map_bands_ids = np.where(glaciers_mask > 0, map_bands_ids, 0)

        band_ids = np.unique(map_bands_ids)
        band_ids = band_ids[band_ids != 0]

        glacier_patches = []
        for band_id in band_ids:
            mask_band = map_bands_ids == band_id

            # Get the hydro unit ids for the corresponding band
            unit_ids = np.unique(catchment.map_unit_ids[mask_band])
            unit_ids = unit_ids[unit_ids != 0]

            for unit_id in unit_ids:
                mask_unit_id = catchment.map_unit_ids[mask_band] == unit_id
                area = np.count_nonzero(mask_unit_id) * px_area
                glacier_patches.append((band_id, unit_id, area))

        return glacier_patches

    @staticmethod
    def _mask_dem(
            catchment: Catchment,
            shapefile: hb.gpd.GeoDataFrame,
            nodata: int = -9999
    ) -> np.ndarray:
        geoms = []
        for geo in shapefile.geometry.values:
            geoms.append(mapping(geo))
        dem_masked, _ = mask(catchment.dem, geoms, crop=False, all_touched=True)
        dem_masked[dem_masked == catchment.dem.nodata] = nodata
        if len(dem_masked.shape) == 3:
            dem_masked = dem_masked[0]

        return dem_masked

    @staticmethod
    def _simplify_df_geometries(df: hb.gpd.GeoDataFrame) -> hb.gpd.GeoDataFrame:
        # Merge the polygons
        df['new_col'] = 0
        df = df.dissolve(by='new_col', as_index=False)
        # Drop all columns except the geometry
        df = df[['geometry']]

        return df
