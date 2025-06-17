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
        self.elevation_bands = None

        # Tables
        self.lookup_table_area = None
        self.lookup_table_volume = None
        self.we = None
        self.areas_perc = None

        # Delta-h parametrization based on Huss et al. (2010).
        self.a_coeff = np.nan
        self.b_coeff = np.nan
        self.c_coeff = np.nan
        self.gamma_coeff = np.nan

        # Method-related attributes
        self.initial_total_glacier_we = np.nan
        self.norm_elevations = np.nan
        self.norm_delta_we = np.nan
        self.scaling_factor_mm = np.nan
        self.excess_melt_we = 0
        
        # Areas based on topography
        self.ice_thicknesses = [{}]
        self.glacier_patches = None

    def compute_initial_ice_thickness(
            self,
            catchment: Catchment,
            glacier_outline: str | None = None,
            ice_thickness: str | None = None,
            elevation_bands_distance: int = 10,
            area_from_topo: bool = True
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
            Path to the TIF file containing the glacier thickness.
            If None, the ice thickness is estimated based on the glacier area
            using the Bahr et al. (1997) formula.
            Either this or glacier_outline should be provided.
        elevation_bands_distance
            Distance between elevation bands in meters. Default is 10 m.

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

        # Discretize the DEM into elevation bands at the given distance
        elevations, map_bands_ids = self._discretize_elevation_bands(
            catchment, elevation_bands_distance)

        # Extract the ice thickness from a TIF file created either from geophysical
        # measurements or calculated based on an inversion of surface topography
        # (Farinotti et al., 2009a,b; Huss et al., 2010)).
        if ice_thickness is not None:
            if not hb.has_pyproj:
                raise ImportError("pyproj is required to do this.")

            # Extract the ice thickness and resample it to the DEM resolution
            catchment.extract_attribute_raster(
                ice_thickness, 'ice_thickness', resample_to_dem_resolution=True,
                resampling='average')
            ice_thickness = catchment.attributes['ice_thickness']['data']
            ice_thickness[catchment.dem_data == 0] = 0.0

            glaciers_mask = np.zeros(catchment.dem_data.shape)
            glaciers_mask[ice_thickness > 0] = 1
        else:
            # Extract the glacier cover from the shapefile
            glaciers_mask = self._extract_glacier_mask_from_shapefile(
                catchment, glacier_outline)

        glacier_patches = self._get_glacier_patches(catchment, map_bands_ids,
                                                    glaciers_mask)
        self.glacier_patches = glacier_patches

        # Create the dataframe for the glacier data
        glacier_df = None
        for band_id, unit_id, area in glacier_patches:
            new_row = pd.DataFrame(
                [[band_id, elevations[band_id - 1], area, 0.0, unit_id]],
                columns=[('band_id', '-'),
                         ('elevation', 'm'),
                         ('glacier_area', 'm2'),
                         ('glacier_thickness', 'm'),
                         ('hydro_unit_id', '-')])
            if glacier_df is None:
                glacier_df = new_row
            else:
                glacier_df = pd.concat([glacier_df, new_row], ignore_index=True)

        # Extract the ice thickness from a TIF file created either from geophysical
        # measurements or calculated based on an inversion of surface topography
        # (Farinotti et al., 2009a,b; Huss et al., 2010)).
        if ice_thickness is not None:
            # Update the dataframe with the ice thickness
            for i, row in glacier_df.iterrows():
                band_id = row[('band_id', '-')]
                unit_id = row[('hydro_unit_id', '-')]

                # Get the ice thickness for the corresponding band and unit
                mask_band = map_bands_ids == band_id
                mask_unit = catchment.map_unit_ids == unit_id
                masked_thickness = ice_thickness[mask_band & mask_unit]
                if area_from_topo:
                    self.ice_thicknesses[0][unit_id] = masked_thickness
                masked_thickness = masked_thickness[masked_thickness > 0]

                # Compute the mean thickness
                if masked_thickness.size > 0 and not np.isnan(masked_thickness).all():
                    mean_thickness = round(float(np.nanmean(masked_thickness)), 3)
                else:
                    mean_thickness = 0.0

                glacier_df.at[i, ('glacier_thickness', 'm')] = mean_thickness

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
            glacier_profile_csv: str | None = None,
            glacier_df: pd.DataFrame | None = None,
            nb_increments: int = 100,
            update_width: bool = True,
            update_width_reference: str = 'initial',
            area_from_topo: bool = True
    ):
        """
        Prepare the glacier mass balance lookup table. The glacier mass balance is
        calculated using the delta-h method (Huss et al., 2010) using the
        lookup table method (Seibert et al., 2018). The glacier mass balance is
        calculated for each elevation band and melt increment and is synthesized
        in a lookup table per hydro unit.

        Parameters
        ----------
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
            2018). Default is True.
        update_width_reference
            Reference for updating the glacier width (Eq. 7 Seibert et al., 2018).
            Default is 'initial'. Options are:
            - 'initial': Use the initial glacier width.
            - 'previous': Use the glacier width from the previous iteration.
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

        # We have to remove the bands with no glacier area
        # (otherwise the min and max elevations are wrong)
        self.glacier_df = self.glacier_df.drop(
            self.glacier_df[self.glacier_df[('glacier_area', 'm2')] == 0].index)

        # Extract the relevant columns
        elevation_bands = self.glacier_df[('elevation', 'm')].values
        initial_areas_m2 = self.glacier_df[('glacier_area', 'm2')].values
        initial_we_mm = self.glacier_df[
                            ('glacier_thickness', 'm')].values * ICE_WE * 1000
        hydro_unit_ids = self.glacier_df[('hydro_unit_id', '-')].values

        nb_elevation_bands = len(elevation_bands)

        self.excess_melt_we = 0
        self.elevation_bands = elevation_bands
        self.hydro_unit_ids = hydro_unit_ids
        self.we = np.zeros((nb_increments + 1, nb_elevation_bands))
        self.we[0] = initial_we_mm  # Initialization
        self.areas_perc = np.zeros((nb_increments + 1, nb_elevation_bands))
        self.areas_perc[0] = initial_areas_m2 / self.catchment_area  # Initialization
        self.lookup_table_area = np.zeros((nb_increments + 1,
                                           len(np.unique(hydro_unit_ids))))
        self.lookup_table_volume = np.zeros((nb_increments + 1,
                                             len(np.unique(hydro_unit_ids))))
        for _ in range(nb_increments):
            self.ice_thicknesses.append({})
        
        # In case the catchment is discretized by radiation or aspect
        self.unique_elevation_bands, self.inverse_indices = np.unique(elevation_bands, return_inverse=True)
        nb_unique_elevation_bands = len(self.unique_elevation_bands)
        # Loop over elevation groups and sum relevant columns
        self.elev_band_areas_perc = np.zeros((nb_increments + 1, nb_unique_elevation_bands))
        self.elev_band_we = np.zeros((nb_increments + 1, nb_unique_elevation_bands))
        for i, elev_idx in enumerate(np.arange(nb_unique_elevation_bands)):
            mask = self.inverse_indices == elev_idx  # bands with this elevation
            self.elev_band_areas_perc[0, elev_idx] = self.areas_perc[0, mask].sum()
            percentages = self.areas_perc[0, mask] / self.areas_perc[0, mask].sum()
            self.elev_band_we[0, elev_idx] = np.sum(self.we[0, mask] * percentages)

        self._initialization()

        for increment in range(1, nb_increments):  # last row kept with zeros
            self._compute_delta_h(increment, nb_increments)
            self._update_glacier_thickness(increment, area_from_topo)
            self._width_scaling(increment, update_width, update_width_reference)

        if not update_width:
            self._final_width_scaling(nb_increments)

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
            columns=np.unique(self.hydro_unit_ids))

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
            columns=np.unique(self.hydro_unit_ids))

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
            columns=np.unique(self.hydro_unit_ids))
        lookup_table_area.to_csv(
            output_dir / "glacier_evolution_lookup_table_area.csv", index=False)

        lookup_table_volume = pd.DataFrame(
            self.lookup_table_volume,
            index=range(self.lookup_table_volume.shape[0]),
            columns=np.unique(self.hydro_unit_ids))
        lookup_table_volume.to_csv(
            output_dir / "glacier_evolution_lookup_table_volume.csv", index=False)

        if self.areas_perc is not None:
            columns = pd.MultiIndex.from_arrays([range(len(self.areas_perc[0])),
                                                 self.hydro_unit_ids, 
                                                 self.elevation_bands], 
                                                 names=['id', 
                                                        'hydro_unit_id', 
                                                        'elevation_band'])
            details_glacier_areas = pd.DataFrame(
                self.areas_perc * self.catchment_area,
                index=range(self.areas_perc.shape[0]),
                columns=columns)
            details_glacier_areas.to_csv(
                output_dir / "details_glacier_areas_evolution.csv", index=False)

        if self.we is not None:
            columns = pd.MultiIndex.from_arrays([range(len(self.we[0])),
                                                 self.hydro_unit_ids, 
                                                 self.elevation_bands], 
                                                 names=['id', 
                                                        'hydro_unit_id', 
                                                        'elevation_band'])
            details_glacier_we = pd.DataFrame(
                self.we,
                index=range(self.we.shape[0]),
                columns=columns)
            details_glacier_we.to_csv(
                output_dir / "details_glacier_we_evolution.csv", index=False)

    def _initialization(self):
        """
        Step 1 of Seibert et al. (2018): Calculation of the initial total glacier mass
        and normalization of glacier elevations. Selection of the right glacial
        parametrization (delta-h method).
        """
        # Calculate total glacier mass (Eq. 2)
        # areas: area (expressed as a proportion of the catchment area) for each
        # elevation band i
        self.initial_total_glacier_we = np.sum(self.areas_perc[0] * self.we[0, :])

        # Normalize glacier elevations (Eq. 3)
        # elevations: absolute elevation for each elevation band i
        max_elevation_m = np.max(self.unique_elevation_bands)
        min_elevation_m = np.min(self.unique_elevation_bands)
        self.unique_norm_elevations = ((max_elevation_m - self.unique_elevation_bands) /
                                       (max_elevation_m - min_elevation_m))
        self.norm_elevations = ((max_elevation_m - self.elevation_bands) /
                                (max_elevation_m - min_elevation_m))

        # Choose the glacial parametrization (Huss et al., 2010, Fig. 2a)
        glacier_area_km2 = (np.sum(self.areas_perc[0]) *
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
        self.norm_delta_we = (
                np.power(self.norm_elevations + self.a_coeff, self.gamma_coeff) +
                self.b_coeff * (self.norm_elevations + self.a_coeff) +
                self.c_coeff)
        self.unique_norm_delta_we = (
                np.power(self.unique_norm_elevations + self.a_coeff, self.gamma_coeff) +
                self.b_coeff * (self.unique_norm_elevations + self.a_coeff) +
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
            np.sum(self.elev_band_areas_perc[increment - 1] * self.unique_norm_delta_we))

    def _update_glacier_thickness(self, increment: int, area_from_topo: bool):
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
        # Glacier water equivalent reduction
        we_reduction_mm = self.scaling_factor_mm * self.norm_delta_we
        
        if area_from_topo:
            
            # Extract the bands IDs that present some glacier surface, and get their numbers
            glacier_band_ids = [patch[0] for patch in self.glacier_patches]
            nb_glacier_band_ids = len(glacier_band_ids)
            
            # Compute the melt per band
            band_excess_melt = np.zeros(nb_glacier_band_ids)
            for band_id in range(nb_glacier_band_ids):
                if band_id in self.ice_thicknesses[increment - 1]:
                    new_ice_thicknesses = self.ice_thicknesses[increment - 1][band_id] - we_reduction_mm[band_id]
            
                    self.ice_thicknesses[increment][band_id] = np.where(new_ice_thicknesses < 0, 0, new_ice_thicknesses)
            
                    band_excess_melt[band_id] = np.sum(np.where(new_ice_thicknesses < 0, new_ice_thicknesses, 0))
                else:
                    band_excess_melt[band_id] = 0
                    
            # This excess melt is taken into account in Step 2.
            self.excess_melt_we = - np.sum(band_excess_melt * self.areas_perc[increment - 1])
            
        else:
            new_we = self.we[increment - 1] - we_reduction_mm
    
            self.we[increment] = np.where(new_we < 0, 0, new_we)
    
            # This excess melt is taken into account in Step 2.
            self.excess_melt_we = - np.sum(np.where(new_we < 0, new_we, 0) *
                                           self.areas_perc[increment - 1])

    def _width_scaling(
            self,
            increment: int,
            update_width: bool,
            update_width_reference: str = 'initial'
    ):
        """
        Step 4 of Seibert et al. (2018): "The width scaling within each elevation band
        relates a decrease in glacier thickness to a reduction of the glacier area
        within the respective elevation band. In other words, this approach also allows
        for glacier area shrinkage at higher elevations, which mimics the typical
        spatial effect of the downwasting of glaciers. Relation from Bahr et al. (1997)."
        """
        # Width scaling (Eq. 7)
        if update_width:
            if update_width_reference == 'previous':
                pos_we = self.we[increment - 1] > 0
                self.areas_perc[increment, pos_we] = (
                        self.areas_perc[increment - 1, pos_we] * np.power(
                    self.we[increment, pos_we] / self.we[increment - 1, pos_we], 0.5))

            elif update_width_reference == 'initial':
                pos_we = self.we[0] > 0
                self.areas_perc[increment, pos_we] = (
                        self.areas_perc[0, pos_we] * np.power(
                    self.we[increment, pos_we] / self.we[0, pos_we], 0.5))

            else:
                raise ValueError("update_width_reference should be either 'previous' "
                                 "or 'initial'.")
                    
            # Conservation of the w.e.
            pos_we = self.we[increment] > 0
            self.we[increment, pos_we] *= (self.areas_perc[increment - 1, pos_we] /
                                           self.areas_perc[increment, pos_we])
            
            # Nullify the areas of the elevation bands with no glacier water equivalent
            self.areas_perc[increment, self.we[increment] == 0] = 0
            
            # TODO: No radiation implemented in this option yet.
            try:
                if len(self.elev_band_areas_perc[increment]) != len(self.areas_perc[increment]):
                    raise ValueError("Discretization by radiation or aspect is not implemented with the option update_width=True.")
            except ValueError as e:
                print(e)
                raise
            self.elev_band_areas_perc[increment] = self.areas_perc[increment]
        else:
            # If the glacier width is not updated, keep the previous glacier area.
            self.elev_band_areas_perc[increment] = self.elev_band_areas_perc[increment - 1]
            self.areas_perc[increment] = self.areas_perc[increment - 1]
            # Update
            for elev_idx in range(len(self.unique_elevation_bands)):
                band_mask = self.inverse_indices == elev_idx
                if self.areas_perc[increment - 1, band_mask].sum() == 0:
                    percentages = 0
                else:
                    percentages = self.areas_perc[increment - 1, band_mask] / self.areas_perc[increment - 1, band_mask].sum()
                self.elev_band_we[increment, elev_idx] = np.sum(self.we[increment, band_mask] * percentages)
                # Nullify the areas of the elevation bands with no glacier water equivalent
                if self.elev_band_we[increment, elev_idx] == 0:
                    self.areas_perc[increment, band_mask] = 0
            # Nullify the areas of the elevation bands with no glacier water equivalent
            self.elev_band_areas_perc[increment, self.elev_band_we[increment] == 0] = 0

    def _final_width_scaling(self, nb_increments):
        """
        Similar to _width_scaling, but for the case when the glacier area is not
        updated during the loop.
        """
        # Compute the elevation band areas from the aspect/radiation discretization.
        nb_elevation_bands = len(self.unique_elevation_bands)
        
        # Width scaling (Eq. 7)
        for i in range(1, len(self.elev_band_we)):
            mask = self.elev_band_we[0] > 0
            self.elev_band_areas_perc[i, mask] = (
                    self.elev_band_areas_perc[0, mask] * np.power(
                self.elev_band_we[i, mask] / self.elev_band_we[0, mask], 0.5))

            # Conservation of the w.e.
            mask = self.elev_band_we[i] > 0
            self.elev_band_we[i, mask] *= (self.elev_band_areas_perc[0, mask] /
                                           self.elev_band_areas_perc[i, mask])
            
        # Redistribution following the aspect/radiation discretization.
        for increment in range(1, nb_increments):
            for i, elev_idx in enumerate(np.arange(len(self.unique_elevation_bands))):
                band_mask = self.inverse_indices == elev_idx
                # Scaling each aspect/radiation area in the total band proportionally
                self.areas_perc[increment, band_mask] *= self.elev_band_areas_perc[increment, elev_idx] / self.elev_band_areas_perc[0, elev_idx]
                # Adjusting the thickness so that the water equivalent remains constant
                if self.elev_band_areas_perc[increment, elev_idx] == 0:
                    self.we[increment, band_mask] = 0
                else:
                    self.we[increment, band_mask] *= self.elev_band_areas_perc[0, elev_idx] / self.elev_band_areas_perc[increment, elev_idx]

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
                areas = self.areas_perc[:, indices] * self.catchment_area
                self.lookup_table_area[:, i] = np.sum(areas, axis=1)
                volumes = self.we[:, indices] * areas / (ICE_WE * 1000)
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
        first_band = hydro_units.iloc[0]
        hu_steps = (first_band['elevation_max'] - first_band['elevation_min']).values[0]
        if hu_steps % elevation_bands_distance != 0:
            raise ValueError(f"Hydro unit elevation range ({hu_steps}) must be a "
                             f"multiple of the elevation bands distance "
                             f"({elevation_bands_distance}). Please adjust the "
                             f"elevation_bands_distance parameter.")

        # Discretize the DEM into elevation bands at the given distance
        min_elevation = hydro_units['elevation_min'].min().values[0]
        max_elevation = hydro_units['elevation_max'].max().values[0]
        elevations = np.arange(min_elevation, max_elevation + elevation_bands_distance,
                               elevation_bands_distance)

        map_bands_ids = np.zeros(catchment.dem_data.shape)
        for i in range(len(elevations) - 1):
            val_min = elevations[i]
            val_max = elevations[i + 1]
            mask_band = np.logical_and(
                catchment.dem_data >= val_min, catchment.dem_data < val_max)
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
