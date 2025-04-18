from pathlib import Path

import numpy as np
import pandas as pd

import hydrobricks as hb
from hydrobricks.constants import WATER_EQ


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

    def __init__(self, hydro_units):
        """
        Initialize the GlacierMassBalance class.

        Parameters
        ----------
        hydro_units : hb.HydroUnit
            The hydro unit object.
        """
        self.hydro_units = hydro_units.hydro_units
        self.catchment_area = np.sum(self.hydro_units.area.values)
        self.hydro_unit_ids = None
        self.elevation_bands = None

        # Tables
        self.lookup_table = None
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

    def compute_lookup_table(self, glacier_data_csv, nb_increments=100,
                             update_width=True, update_width_reference='initial'):
        """
        Prepare the glacier mass balance lookup table. The glacier mass balance is
        calculated using the delta-h method (Huss et al., 2010) using the
        lookup table method (Seibert et al., 2018). The glacier mass balance is
        calculated for each elevation band and melt increment and is synthesized
        in a lookup table per hydro unit.

        Parameters
        ----------
        glacier_data_csv : str
            Path to the CSV file containing glacier data. An elevation band is smaller
            than a hydro unit, usually around 10 m high. It should contain the
            following columns:
            - elevation: Elevation (m) for each elevation band i (e.g. every 10m).
            - glacier_area: Glacier area (m2) for each elevation band i.
            - glacier_thickness: Glacier thickness (m) for each elevation band i.
            - hydro_unit_id: Hydro unit ID for each elevation band i.

            Format example:
            elevation   glacier_area    glacier_thickness   hydro_unit_id
            m           m2              m                   2
            1670        0               0                   2
            1680        0               0                   2
            1690        2500            14.7                2
            1700        9375            23.2                3
            1710        11875           25.2                3
            1720        9375            23.2                3
            1730        9375            23.2                3
            ...         ...             ...                 ...

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
        """
        # Read the glacier data
        glacier_df = pd.read_csv(glacier_data_csv, skiprows=[1])

        # We have to remove the bands with no glacier area
        # (otherwise the min and max elevations are wrong)
        glacier_df = glacier_df.drop(glacier_df[glacier_df.glacier_area == 0].index)

        # Extract the relevant columns
        elevation_bands = glacier_df['elevation'].values
        initial_areas_m2 = glacier_df['glacier_area'].values
        initial_we_mm = glacier_df['glacier_thickness'].values * WATER_EQ * 1000
        hydro_unit_ids = glacier_df['hydro_unit_id'].values

        nb_elevation_bands = len(elevation_bands)

        self.excess_melt_we = 0
        self.elevation_bands = elevation_bands
        self.hydro_unit_ids = hydro_unit_ids
        self.we = np.zeros((nb_increments + 1, nb_elevation_bands))
        self.we[0] = initial_we_mm  # Initialization
        self.areas_perc = np.zeros((nb_increments + 1, nb_elevation_bands))
        self.areas_perc[0] = initial_areas_m2 / self.catchment_area  # Initialization
        self.lookup_table = np.zeros((nb_increments + 1,
                                      len(np.unique(hydro_unit_ids))))

        self._initialization()

        for increment in range(1, nb_increments):  # last row kept with zeros
            self._compute_delta_h(increment, nb_increments)
            self._update_glacier_thickness(increment)
            self._width_scaling(increment, update_width, update_width_reference)

        if not update_width:
            self._final_width_scaling()

        self._update_hydro_unit_glacier_areas()

    def save_as_csv(self, output_dir):
        output_dir = Path(output_dir)

        # Write record to the lookup table
        lookup_table = pd.DataFrame(
            self.lookup_table,
            index=range(self.lookup_table.shape[0]),
            columns=np.unique(self.hydro_unit_ids))
        lookup_table.to_csv(
            output_dir / "glacier_evolution_lookup_table.csv")

        if self.areas_perc is not None:
            details_glacier_areas = pd.DataFrame(
                self.areas_perc * self.catchment_area,
                index=range(self.areas_perc.shape[0]),
                columns=range(len(self.areas_perc[0])))
            details_glacier_areas.to_csv(
                output_dir / "details_glacier_areas_evolution.csv")

        if self.we is not None:
            details_glacier_we = pd.DataFrame(
                self.we,
                index=range(self.we.shape[0]),
                columns=range(len(self.we[0])))
            details_glacier_we.to_csv(
                output_dir / "details_glacier_we_evolution.csv")

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
        max_elevation_m = np.max(self.elevation_bands)
        min_elevation_m = np.min(self.elevation_bands)
        self.norm_elevations = ((max_elevation_m - self.elevation_bands) /
                                (max_elevation_m - min_elevation_m))

        # Choose the glacial parametrization (Huss et al., 2010, Fig. 2a)
        glacier_area_km2 = (np.sum(self.areas_perc[0]) *
                            self.catchment_area / (1000 * 1000))
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

    def _compute_delta_h(self, increment, nb_increments):
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
        glacier_we_change_mm = (self.initial_total_glacier_we / nb_increments +
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

    def _width_scaling(self, increment, update_width, update_width_reference):
        """
        Step 4 of Seibert et al. (2018): "The width scaling within each elevation band
        relates a decrease in glacier thickness to a reduction of the glacier area
        within the respective elevation band. In other words, this approach also allows
        for glacier area shrinkage at higher elevations, which mimics the typical
        spatial effect of the downwasting of glaciers."
        """
        # Width scaling (Eq. 7)
        if update_width:
            if update_width_reference == 'previous':
                mask = self.we[increment - 1] > 0
                self.areas_perc[increment, mask] = (
                        self.areas_perc[increment - 1, mask] * np.power(
                    self.we[increment, mask] / self.we[increment - 1, mask], 0.5))

            elif update_width_reference == 'initial':
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
                self.lookup_table[:, i] = np.sum(
                    self.areas_perc[:, indices], axis=1) * self.catchment_area
            else:
                self.lookup_table[:, i] = 0
