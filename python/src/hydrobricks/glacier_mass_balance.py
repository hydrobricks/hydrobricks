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
                 scale_width=True):
        # Basic properties
        self.elevation_bands_m = glacier_df['elevation_bands_m']
        self.hydro_unit_id = glacier_df['elevation_zone_id']
        self.nb_increments = nb_increments
        self.scale_width = scale_width
        self.catchment_area_m2 = catchment_area_m2
        nb_elevation_bands = len(self.elevation_bands_m)
        nb_hydro_units = len(np.unique(self.hydro_unit_id))

        # Delta-H parametrization based on Huss et al. (2010).
        self.a_coeff = np.nan
        self.b_coeff = np.nan
        self.c_coeff = np.nan
        self.gamma_coeff = np.nan

        # Step 1
        # Water equivalent in mm (depth) for each elevation band i, and for each increment.
        self.we = np.ones((nb_increments, nb_elevation_bands))
        self.we[0] = glacier_df['initial_water_equivalents_mm']  # Initialization
        self.initial_areas_perc = glacier_df['initial_areas_m2'] / catchment_area_m2
        self.initial_total_glacier_we = np.nan
        self.norm_elevations = np.nan

        # Step 2
        self.norm_delta_we = np.nan
        self.scaling_factor_mm = np.nan

        # Step 3
        self.excess_melt_we = 0

        # Step 4
        self.areas_perc = np.zeros((nb_increments, nb_elevation_bands))
        self.areas_perc[0] = self.initial_areas_perc  # Initialization

        # Step 6
        self.hydro_unit_glacier_areas_m2 = np.ones((nb_increments, nb_hydro_units))
        self._update_hydro_unit_glacier_areas(-1)  # Initialization

        # After loop
        self.scaled_areas_perc = np.zeros((nb_increments, nb_elevation_bands))
        self.scaled_areas_perc[0] = self.initial_areas_perc  # Initialization
        self.scaled_we_mm = np.ones((nb_increments, nb_elevation_bands))
        self.scaled_we_mm[0] = glacier_df['initial_water_equivalents_mm']  # Init.

    def _initialization(self):
        """
        Step 1 of Seibert et al. (2018): Calculation of the initial total glacier mass
        and normalization of glacier elevations. Selection of the right glacial
        parametrization (delta-h method).
        """
        # Calculate total glacier mass (Eq. 2)
        # areas: area (expressed as a proportion of the catchment area) for each
        # elevation band i
        self.initial_total_glacier_we = np.sum(self.initial_areas_perc * self.we[0, :])

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
        mask = self.we[increment - 1] > 0
        self.scaling_factor_mm = glacier_we_change_mm / (
            np.sum(self.areas_perc[increment - 1, mask] * self.norm_delta_we[mask]))

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

    def width_updating(self, increment):
        """
        Step 4 of Seibert et al. (2018):
        "The delta h approach distributes the change in glacier mass over the
        different elevation zones though it results in glacier-free areas mainly
        at the lowest elevations. The width scaling within each elevation band
        relates a decrease in glacier thickness to a reduction of the glacier area
        within the respective elevation band. In other words, this approach also
        allows for glacier area shrinkage at higher elevations, which mimics
        the typical spatial effect of the downwasting of glaciers." -> THIS WAS
        MISLEADING. THE BAHR SCALING SHOULD BE DONE AFTER THE LOOP, AS IT IS NOT
        NEEDED IN THE LOOP AND SHOULD NOT BE USED AS INPUT OF OTHER CALCULATIONS.
        INSTEAD, WE JUST APPLY THE SURFACE REDUCTION FROM HUSS.
        """

        # Huss "Glacier extent is determined by updating the ice thickness distribution; the glacier disap-
        # pears where ice thickness drops to zero.
        self.areas_perc[increment + 1] = np.where(self.we_mm[increment + 1] == 0,
                                                  0, self.areas_perc[increment])

    def width_scaling(self):
        """
        Step 4 of Seibert et al. (2018):
        "The delta h approach distributes the change in glacier mass over the
        different elevation zones though it results in glacier-free areas mainly
        at the lowest elevations. The width scaling within each elevation band
        relates a decrease in glacier thickness to a reduction of the glacier area
        within the respective elevation band. In other words, this approach also
    def _final_width_scaling(self):
        """
        Similar to _width_scaling, but for the last increment.
        """
        # Width scaling (Eq. 7)
        for i in range(len(self.we)):
            self.scaled_areas_perc[i] = self.initial_areas_perc * np.power(
                self.we[i] / self.we[0], 0.5)
            # If we want to keep the same glacial volume, we need to modify accordingly the thickness
            self.scaled_we_mm[i] = (
                    self.areas_perc[i] * self.we[i] /
                    self.scaled_areas_perc[i])

    def _compute_fractions_per_elevation_zones(self):
        """
        Step 5 of Seibert et al. (2018):
        "Define elevation zones and compute the fractions of glacier and non-glacier
        area (relative to the catchment area) for each elevation zone."
        """

        ### Define elevation zones - Not done here as I suppose they will be defined by us beforehand for Hydrobricks (HRUs)
        # ------

        ### Compute the fraction of glacier and non-glacier areas.
        # fractions = NOT SURE I GET IT...
        return

    def _update_hydro_unit_glacier_areas(self, increment):
        """
        Step 6 of Seibert et al. (2018):
        "Sum the total (width-scaled) areas for all respective elevation bands
        which are covered by glaciers (i.e. glacier water equivalent ≥ 0) for
        each elevation zone."
        """
        # Update elevation zone areas
        for i, elevation_zone in enumerate(np.unique(self.hydro_unit_id)):  # Do not necessarily start with 0.
            indices = np.where(self.hydro_unit_id == elevation_zone)[0]
            if len(indices) != 0:
                self.hydro_unit_glacier_areas_m2[increment, i] = np.sum(
                    self.areas_perc[increment][indices]) * self.catchment_area_m2
            else:
                self.hydro_unit_glacier_areas_m2[increment, i] = 0

    def write_lookup_table(self):
        """
        Step 7 of Seibert et al. (2018):
        "M (in % of initial M) is in the first column, followed by one column
        for each elevation zone with the areal glacier cover area (in % of
        catchment area)."
        """

        ### Write record to the lookup table
        lookup_table = pd.DataFrame(self.hydro_unit_glacier_areas_m2,
                                    index=range(self.nb_increments),
                                    columns=range(len(np.unique(self.hydro_unit_id))))
        lookup_table.to_csv("lookup_table.csv")

        scaled_areas_lookup_table = pd.DataFrame(self.scaled_areas_perc * self.catchment_area_m2,
                                                 index=range(self.nb_increments),
                                                 columns=range(len(self.scaled_areas_perc[0])))
        scaled_areas_lookup_table.to_csv("scaled_areas_lookup_table.csv")

        areas_lookup_table = pd.DataFrame(self.areas_perc * self.catchment_area_m2,
                                          index=range(self.nb_increments),
                                          columns=range(len(self.areas_perc[0])))
        areas_lookup_table.to_csv("areas_lookup_table.csv")

        scaled_we_lookup_table = pd.DataFrame(self.scaled_we_mm / 1000,
                                              index=range(self.nb_increments),
                                              columns=range(len(self.scaled_we_mm[0])))
        scaled_we_lookup_table.to_csv("scaled_we_lookup_table.csv")

        we_lookup_table = pd.DataFrame(self.we / 1000,
                                       index=range(self.nb_increments),
                                       columns=range(len(self.we[0])))
        we_lookup_table.to_csv("we_lookup_table.csv")

    def loop_through_the_steps(self):
        """
        Seibert et al. (2018):
        "Melt the glacier in steps of 1 % of its total mass"
        """
        self._initialization()

        for increment in range(1, self.nb_increments):
            self._compute_delta_h(increment)
            self._update_glacier_thickness(increment)
            self._width_scaling(increment)
            self._compute_fractions_per_elevation_zones()  # I REMOVED THE DEFINITION OF THE ELEVATION ZONES FROM HERE
            self._update_hydro_unit_glacier_areas(increment)  # UNCLEAR # NOT SURE WHY IT SHOULD BE IN THE LOOP...
        #self._final_width_scaling()
        self.write_lookup_table()  # FOR SURE, THIS SHOULDN'T BE IN THE LOOP

        ### Loop through the steps 2, 3, 6 and 7.

    def run(self):
        """
        Step 8 of Seibert et al. (2018):
        "Run once before the actual simulation of the time series starts
        (automatically within the HBV-light software)."
        """

        self.loop_through_the_steps()
        # RUN HYDROBRICKS
        # THIS FUNCTION WILL PROBABLY DISAPPEAR TO BE REPLACED BY A HYDROBRICKS MORE CORE FUNCTION


def plot_figure_2b(elevation_bands):
    """
    Reproduces Figure 2b.
    """

    areas_lookup_table = pd.read_csv("areas_lookup_table.csv", index_col=0)
    we_lookup_table = pd.read_csv("we_lookup_table.csv", index_col=0)
    scaled_areas_lookup_table = pd.read_csv("scaled_areas_lookup_table.csv", index_col=0)

    band_width = elevation_bands[1] - elevation_bands[0]
    all_bands = elevation_bands - band_width / 2
    all_bands = np.append(all_bands, elevation_bands[-1] + band_width / 2)

    path = "/home/anne-laure/eclipse-workspace/Huss_glacial_melt_files/"
    areas_df = pd.read_csv(path + "histogram2_values_corr.csv")
    # Grey lines
    for i in range(len(scaled_areas_lookup_table)):
        if i % 5 == 0:
            trick_to_plot = np.append(scaled_areas_lookup_table.iloc[i, :].values[0],
                                      scaled_areas_lookup_table.iloc[i, :].values)
            plt.plot(trick_to_plot, all_bands, drawstyle="steps-post", color="lightgrey")
    # Black lines
    for i in range(len(scaled_areas_lookup_table)):
        if i % 20 == 0:
            trick_to_plot = np.append(scaled_areas_lookup_table.iloc[i, :].values[0],
                                      scaled_areas_lookup_table.iloc[i, :].values)
            plt.plot(trick_to_plot, all_bands, drawstyle="steps-post", color="black")
    print(areas_df)
    plt.plot(areas_df["Glacier area [m²]"], [x + 5 for x in areas_df["Elevation [m a.s.l.]"]],
             drawstyle="steps-post", color='red')
    plt.xlabel('Glacier area (scaled) (m²)')
    plt.ylabel('Elevation (m a.s.l.)')

    plt.figure()
    # Grey lines
    for i in range(len(scaled_areas_lookup_table)):
        if i % 5 == 0:
            trick_to_plot = np.append(scaled_areas_lookup_table.iloc[i, :].values[0],
                                      scaled_areas_lookup_table.iloc[i, :].values)
            plt.plot(trick_to_plot / areas_df["Glacier area [m²]"][8:-7], all_bands,
                     drawstyle="steps-post", color="lightgrey")
    # Black lines
    for i in range(len(scaled_areas_lookup_table)):
        if i % 20 == 0:
            trick_to_plot = np.append(scaled_areas_lookup_table.iloc[i, :].values[0],
                                      scaled_areas_lookup_table.iloc[i, :].values)
            plt.plot(trick_to_plot / areas_df["Glacier area [m²]"][8:-7], all_bands,
                     drawstyle="steps-post", color="black")
    print(areas_df)
    plt.xlabel('Glacier area (scaled) / Glacier initial area (-)')
    plt.ylabel('Elevation (m a.s.l.)')

    plt.figure()
    thicknesses_df = pd.read_csv(path + "histogram3_values_corr.csv")
    print(thicknesses_df)
    plt.plot(thicknesses_df["Glacier thickness [m]"], [x + 5 for x in thicknesses_df["Elevation [m a.s.l.]"]],
             drawstyle="steps-post", color='black')
    plt.xlabel('Glacier thickness (m)')
    plt.ylabel('Elevation (m a.s.l.)')

    plt.figure()
    volumes_df = pd.read_csv(path + "histogram1_values_corr.csv")
    print(volumes_df)
    plt.plot(volumes_df["Glacier volume [m³]"] * 1000, [x + 5 for x in volumes_df["Elevation [m a.s.l.]"]],
             drawstyle="steps-post", color='black')
    plt.plot(thicknesses_df["Glacier thickness [m]"] * areas_df["Glacier area [m²]"],
             [x + 5 for x in areas_df["Elevation [m a.s.l.]"]], drawstyle="steps-post", color='red')
    plt.xlabel('Glacier volume (m³)')
    plt.ylabel('Elevation (m a.s.l.)')

    plt.figure()
    # Grey lines
    for i in range(len(areas_lookup_table)):
        if i % 5 == 0:
            trick_to_plot = np.append(
                areas_lookup_table.iloc[i, :].values[0] * we_lookup_table.iloc[i, :].values[0] / WATER_EQ,
                areas_lookup_table.iloc[i, :].values * we_lookup_table.iloc[i, :].values / WATER_EQ)
            plt.plot(trick_to_plot, all_bands, drawstyle="steps-post", color="lightgrey")
    # Black lines
    for i in range(len(areas_lookup_table)):
        if i % 20 == 0:
            trick_to_plot = np.append(
                areas_lookup_table.iloc[i, :].values[0] * we_lookup_table.iloc[i, :].values[0] / WATER_EQ,
                areas_lookup_table.iloc[i, :].values * we_lookup_table.iloc[i, :].values / WATER_EQ)
            plt.plot(trick_to_plot, all_bands, drawstyle="steps-post", color="black")
    plt.plot(thicknesses_df["Glacier thickness [m]"] * areas_df["Glacier area [m²]"],
             [x + 5 for x in areas_df["Elevation [m a.s.l.]"]], drawstyle="steps-post", color='red')
    plt.xlabel('Glacier volume (m³)')
    plt.ylabel('Elevation (m a.s.l.)')

    plt.figure()
    for i in range(len(areas_lookup_table)):
        volumes = areas_lookup_table.iloc[i, :].values * we_lookup_table.iloc[i, :].values / WATER_EQ
        if i == 0:
            plt.axhline(y=np.sum(volumes), color='r', linestyle='-')
        plt.plot(i, np.sum(volumes), marker="o", color="black")
    plt.axhline(y=0, color='r', linestyle='-')
    plt.xlabel('Increment')
    plt.ylabel('Glacier volume (m³)')

    plt.show()


def main():
    # Data from Huss and Seibert
    path = "/home/anne-laure/eclipse-workspace/Huss_glacial_melt_files/"
    areas_df = pd.read_csv(path + "histogram2_values_corr.csv")
    thicknesses_df = pd.read_csv(path + "histogram3_values_corr.csv")
    length = int(len(areas_df["Elevation [m a.s.l.]"].values) / 4)  # For steps of 10 m, and zones of 40 m.
    ids = [i for i in range(length) for _ in range(4)]
    catchment_area_m2 = 100000000

    # Water equivalent (WE) conversion + m to mm conversion
    water_equivalents_mm = thicknesses_df["Glacier thickness [m]"].values * WATER_EQ * 1000

    glacier_data = {"elevation_bands_m": areas_df["Elevation [m a.s.l.]"].values,  # Median elevation for each
                    # elevation band i. An elevation band is smaller than a HRU, usually around 10 m high.
                    "elevation_zone_id": ids,  # For each elevation band i, the ID of the corresponding elevation
                    # zone (the HRU).
                    "initial_water_equivalents_mm": water_equivalents_mm,  # WE in mm (depth) for each elevation band i
                    "initial_areas_m2": areas_df["Glacier area [m²]"].values}  # Glaciated areas for each elev. band i

    glacier_df = pd.DataFrame(glacier_data)
    # We have to remove the bands with no glacier area (otherwise the min and max elevations are wrong)
    glacier_df = glacier_df.drop(glacier_df[glacier_df.initial_areas_m2 == 0].index)

    nb_increments = 100  # This means that the lookup table consists of glacier areas per elevation
    # zone for 101 different glacier mass situations, ranging from the initial glacier mass to zero.

    glacier = GlacierMassBalance(glacier_df, catchment_area_m2, nb_increments)
    glacier.loop_through_the_steps()

    plot_figure_2b(elevation_bands=glacier_df.elevation_bands_m.values)
    print("Terminated")


main()
