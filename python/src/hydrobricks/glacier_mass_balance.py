import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from constants import WATER_EQ


class GlacierMassBalance:
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

    def __init__(self, glacier_df, catchment_area_m2, nb_increments=100,
                 update_width=False, update_width_reference='initial',
                 save_to_file=True):
        """
        Initialize the GlacierMassBalance class.

        Parameters
        ----------
        glacier_df : pd.DataFrame
            DataFrame containing glacier data with columns:
            - elevation_bands_m: Elevation bands in meters.
            - elevation_zone_id: ID of the corresponding hydro unit.
            - initial_water_equivalents_mm: Initial w.e. in mm for each elevation band.
            - initial_areas_m2: Initial glaciated areas for each elevation band.
        catchment_area_m2 : float
            Catchment area in square meters.
        nb_increments : int, optional
            Number of increments for glacier mass balance calculation. Default is 100.
        update_width : bool, optional
            Whether to update the glacier width at each iteration (Eq. 7 Seibert et al.,
            2018). Default is True.
        update_width_reference : str, optional
            Reference for updating the glacier width (Eq. 7 Seibert et al., 2018).
            Default is 'initial'. Options are:
            - 'initial': Use the initial glacier width.
            - 'previous': Use the glacier width from the previous iteration.
        save_to_file : bool, optional
            Whether to save the lookup table to a file. Default is True.
        """
        # Options
        self.nb_increments = nb_increments
        self.update_width = update_width
        self.update_width_reference = update_width_reference
        self.save_to_file = save_to_file

        # Basic properties
        self.elevation_bands_m = glacier_df['elevation_bands_m'].values
        self.hydro_unit_ids = glacier_df['elevation_zone_id'].values
        self.catchment_area_m2 = catchment_area_m2
        nb_elevation_bands = len(self.elevation_bands_m)
        nb_hydro_units = len(np.unique(self.hydro_unit_ids))

        # Delta-h parametrization based on Huss et al. (2010).
        self.a_coeff = np.nan
        self.b_coeff = np.nan
        self.c_coeff = np.nan
        self.gamma_coeff = np.nan

        # Step 1
        # Water equivalent in mm (depth) for each elevation band i, and for each increment.
        initial_areas_perc = glacier_df['initial_areas_m2'] / catchment_area_m2
        self.we = np.zeros((nb_increments + 1, nb_elevation_bands))
        self.we[0] = glacier_df['initial_water_equivalents_mm']  # Initialization
        self.initial_total_glacier_we = np.nan
        self.norm_elevations = np.nan

        # Step 2
        self.norm_delta_we = np.nan
        self.scaling_factor_mm = np.nan

        # Step 3
        self.excess_melt_we = 0

        # Step 4
        self.areas_perc = np.zeros((nb_increments + 1, nb_elevation_bands))
        self.areas_perc[0] = initial_areas_perc  # Initialization

        # Step 5
        self.hydro_unit_glacier_areas_m2 = np.zeros((nb_increments + 1, nb_hydro_units))

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
        max_elevation_m = np.max(self.elevation_bands_m)
        min_elevation_m = np.min(self.elevation_bands_m)
        self.norm_elevations = ((max_elevation_m - self.elevation_bands_m) /
                                (max_elevation_m - min_elevation_m))

        # Choose the glacial parametrization (Huss et al., 2010, Fig. 2a)
        glacier_area_km2 = (np.sum(self.areas_perc[0]) *
                            self.catchment_area_m2 / (1000 * 1000))
        self._set_delta_h_parametrization(glacier_area_km2)

    def _set_delta_h_parametrization(self, glacier_area_km2):
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

    def _compute_delta_h(self, increment):
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

        # Calculate the scaling factor fs (Eq. 5)
        # The glacier volume change increment, added with the excess melt that could
        # not be melted in the previous step
        glacier_we_change_mm = (self.initial_total_glacier_we / self.nb_increments +
                                self.excess_melt_we)
        # "A scaling factor fS (mm), which scales the dimensionless ∆h, is computed
        # based on the glacier volume change M and on the area and normalized water
        # equivalent change for each of the elevation bands."
        self.scaling_factor_mm = glacier_we_change_mm / (
            np.sum(self.areas_perc[increment - 1] * self.norm_delta_we))

    def _update_glacier_thickness(self, increment):
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
        new_we = self.we[increment - 1] - we_reduction_mm

        self.we[increment] = np.where(new_we < 0, 0, new_we)

        # This excess melt is taken into account in Step 2.
        self.excess_melt_we = - np.sum(np.where(new_we < 0, new_we, 0) *
                                       self.areas_perc[increment - 1])

    def _width_scaling(self, increment):
        """
        Step 4 of Seibert et al. (2018): "The width scaling within each elevation band
        relates a decrease in glacier thickness to a reduction of the glacier area
        within the respective elevation band. In other words, this approach also allows
        for glacier area shrinkage at higher elevations, which mimics the typical
        spatial effect of the downwasting of glaciers."
        """
        # Width scaling (Eq. 7)
        if self.update_width:
            if self.update_width_reference == 'previous':
                mask = self.we[increment - 1] > 0
                self.areas_perc[increment, mask] = (
                        self.areas_perc[increment - 1, mask] * np.power(
                    self.we[increment, mask] / self.we[increment - 1, mask], 0.5))

            elif self.update_width_reference == 'initial':
                mask = self.we[0] > 0
                self.areas_perc[increment, mask] = (
                        self.areas_perc[0, mask] * np.power(
                    self.we[increment, mask] / self.we[0, mask], 0.5))

            else:
                raise ValueError("update_width_reference should be either 'previous' "
                                 "or 'initial'.")

            # Conservation of the w.e.
            mask = self.we[increment] > 0
            self.we[increment, mask] *= (self.areas_perc[increment - 1, mask] /
                                         self.areas_perc[increment, mask])
        else:
            # If the glacier width is not updated, keep the previous glacier area.
            self.areas_perc[increment] = self.areas_perc[increment - 1]

        # Nullify the areas of the elevation bands with no glacier water equivalent
        self.areas_perc[increment, self.we[increment] == 0] = 0

    def _final_width_scaling(self):
        """
        Similar to _width_scaling, but for the case when the glacier area is not
        updated during the loop.
        """
        if self.update_width:
            # Already done.
            return

        # Width scaling (Eq. 7)
        for i in range(1, len(self.we)):
            mask = self.we[0] > 0
            self.areas_perc[i, mask] = (
                    self.areas_perc[0, mask] * np.power(
                self.we[i, mask] / self.we[0, mask], 0.5))

            # Conservation of the w.e.
            mask = self.we[i] > 0
            self.we[i, mask] *= (self.areas_perc[0, mask] /
                                 self.areas_perc[i, mask])

    def _update_hydro_unit_glacier_areas(self):
        """
        Step 5 (6) of Seibert et al. (2018): "Sum the total (width-scaled) areas for all
        respective elevation bands which are covered by glaciers (i.e. glacier water
        equivalent ≥ 0) for each elevation zone."
        """
        # Update elevation zone areas
        for i, hydro_unit_id in enumerate(np.unique(self.hydro_unit_ids)):
            indices = np.where(self.hydro_unit_ids == hydro_unit_id)[0]
            if len(indices) != 0:
                self.hydro_unit_glacier_areas_m2[:, i] = np.sum(
                    self.areas_perc[:, indices], axis=1) * self.catchment_area_m2
            else:
                self.hydro_unit_glacier_areas_m2[:, i] = 0

    def _save_to_file(self):
        """
        Step 7 of Seibert et al. (2018): "M (in % of initial M) is in the first column,
        followed by one column for each elevation zone with the areal glacier cover
        area (in % of catchment area)."
        """
        if not self.save_to_file:
            return

        # Write record to the lookup table
        lookup_table = pd.DataFrame(
            self.hydro_unit_glacier_areas_m2,
            index=range(self.nb_increments + 1),
            columns=np.unique(self.hydro_unit_ids))
        lookup_table.to_csv("glacier_evolution_lookup_table.csv")

        areas_lookup_table = pd.DataFrame(
            self.areas_perc * self.catchment_area_m2,
            index=range(self.nb_increments + 1),
            columns=range(len(self.areas_perc[0])))
        areas_lookup_table.to_csv("details_glacier_areas_evolution.csv")

        we_lookup_table = pd.DataFrame(
            self.we,
            index=range(self.nb_increments + 1),
            columns=range(len(self.we[0])))
        we_lookup_table.to_csv("details_glacier_we_evolution.csv")

    def loop_through_the_steps(self):
        """
        Seibert et al. (2018): "Melt the glacier in steps of 1 % of its total mass"
        """
        self._initialization()

        for increment in range(1, self.nb_increments):  # last row kept with zeros
            self._compute_delta_h(increment)
            self._update_glacier_thickness(increment)
            self._width_scaling(increment)

        self._final_width_scaling()
        self._update_hydro_unit_glacier_areas()
        self._save_to_file()
