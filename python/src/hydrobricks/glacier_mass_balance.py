import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from constants import WATER_EQ

class GlacierMassBalance:
    """Class for glacier mass balance"""
    
    def __init__(self, glacier_df, elevation_zones, nb_increments, catchment_area_m2):
        
        # Basic stuff
        self.elevation_bands_m = glacier_df['elevation_bands_m']
        self.elevation_zone_id = glacier_df['elevation_zone_id']
        self.elevation_zones = elevation_zones
        nb_elevation_bands = len(self.elevation_bands_m)
        nb_elevation_zones = len(self.elevation_zones)
        self.nb_increments = nb_increments
        self.catchment_area_m2 = catchment_area_m2
        
        # Step 1 
        # Water equivalent in mm (depth) for each elevation band i, and for each increment.
        self.water_equivalents_mm = np.ones((nb_increments, nb_elevation_bands))
        self.water_equivalents_mm[0] = glacier_df['initial_water_equivalents_mm']  # Initialization
        self.initial_areas_perc = glacier_df['initial_areas_m2'] / catchment_area_m2
        self.total_glacier_mass_mm = np.nan
        self.normalized_elevations_o = np.nan
        
        # Step 2
        self.delta_water_equivalents_o = np.nan
        self.scaling_factor_mm = np.nan
        # Delta-H parametrization based on Huss et al. (2010).
        self.a_coef = np.nan
        self.b_coef = np.nan
        self.c_coef = np.nan
        self.gamma_coef = np.nan
        
        # Step 3
        self.excess_melt_mass_mm = 0
        
        # Step 4
        self.areas_perc = np.zeros((nb_increments, nb_elevation_bands))
        self.areas_perc[0] = self.initial_areas_perc  # Initialization
        
        # Step 6
        self.elevation_zone_areas_m2 = np.ones((nb_increments, nb_elevation_zones))
        self.update_elevation_zones_areas(-1)  # Initialization
        
        # After loop
        self.scaled_areas_perc = np.zeros((nb_increments, nb_elevation_bands))
        self.scaled_areas_perc[0] = self.initial_areas_perc  # Initialization
        self.scaled_water_equivalents_mm = np.ones((nb_increments, nb_elevation_bands))
        self.scaled_water_equivalents_mm[0] = glacier_df['initial_water_equivalents_mm']  # Initialization
        

    def compute_initial_state(self):
        """
        Computes the glacier thickness per band from the glacier thickness.
        
        Step 1 of Seibert et al. (2018):
        "Elevation bands and corresponding water equivalent are given, with
        elevation bands at a ﬁner resolution than the elevation zones. While
        the areal distribution of a static glacier is speciﬁed in HBV-light
        by means of elevation and aspect zones, for establishing the relationship
        between glacier mass and glacier area, a glacier proﬁle, which deﬁnes
        the initial thickness (in mm water equivalent) and areal distribution of
        the glacier at a ﬁner resolution, is needed as model input data. Note that
        the resolution of the glacier routine simulations largely depends on
        the number of elevation bands per elevation zone; i.e. all glacier area
        within each band is either covered by a glacier or not, and the percentage
        of glacierized area within a certain elevation zone is based on the state
        of the individual elevation bands within that elevation zone. Elevation
        zones typically have resolutions of 100 to 200 m, whereas for the elevation
        bands a resolution of 10 m is commonly used."
        """
        
        ### Calculate total glacier mass (Eq. 2)
        # areas: area (expressed as a proportion of the catchment area) for each elevation band i
        self.total_glacier_mass_mm = np.sum(self.initial_areas_perc * self.water_equivalents_mm[0])
        
        ### Normalize glacier elevations (Eq. 3)
        # elevations: absolute elevation for each elevation band i
        max_elevation_m = np.max(self.elevation_bands_m)
        min_elevation_m = np.min(self.elevation_bands_m)
        self.normalized_elevations_o = (max_elevation_m - self.elevation_bands_m) / (max_elevation_m - min_elevation_m)
        
    def parametrization(self, glacier_area_km2):
        """
        Delta-h parametrization of the glacier.
        See Huss et al. (2010).
        
        COMPLETE REFERENCE TO HUSS HERE
        """
        
        if glacier_area_km2 > 20: # Large valley glacier
            self.a_coef = -0.02
            self.b_coef = 0.12
            self.c_coef = 0.00
            self.gamma_coef = 6
        elif glacier_area_km2 >= 5 and glacier_area_km2 <= 20: # Medium valley glacier
            self.a_coef = -0.05
            self.b_coef = 0.19
            self.c_coef = 0.01
            self.gamma_coef = 4
        elif glacier_area_km2 > 0 and glacier_area_km2 < 5: # Small glacier
            self.a_coef = -0.30
            self.b_coef = 0.60
            self.c_coef = 0.09
            self.gamma_coef = 2
        else:
            # THROW AN ERROR!!!?
            pass
        
    def calculate_total_glacier_mass(self, increment):
        """
        Step 2 of Seibert et al. (2018):
        "Depending on the glacier area, select one of the three parameterizations
        suggested by Huss et al. (2010)."
        """
        
        ### Choose the right glacial parametrization (Huss et al., 2010, Fig. 2a)
        # "Based on the initial total glacier area (in km2) that needs to be speciﬁed in addition to the initial
        # glacier thickness proﬁle, one of the three empirical parameterizations applicable for unmeasured glaciers from Huss et
        # al. (2010) is used (Figs. 1 and 2a)."
        glacier_area_km2 = np.sum(self.areas_perc[0]) * self.catchment_area_m2 / (1000*1000)
        self.parametrization(glacier_area_km2)
        print("glacier area km2:", glacier_area_km2)
        
        ### Apply ∆h-parameterization (Eq. 4)
        # gives the normalized (dimensionless) ice thickness change for each elevation band i
        self.delta_water_equivalents_o = np.power(self.normalized_elevations_o + self.a_coef, self.gamma_coef) + \
                                                self.b_coef * (self.normalized_elevations_o + self.a_coef) + self.c_coef \
                                                #- 0.005006
        ### Calculate the scaling factor fs (Eq. 5)
        # the glacier volume change increment, added with the excess melt that could not be melted in the previous step
        delta_mass_balance_mm = self.total_glacier_mass_mm / 100 + self.excess_melt_mass_mm
        # "A scaling factor fS (mm), which scales the dimensionless h, is computed based on the glacier volume
        # change M and on the area and normalized water equivalent change for each of the elevation bands."
        # Huss: "Ba (in kg) is given by the mass balance computation. fs (in m) is a factor that scales the magnitude of the dimension-
        # less ice thickness change (ordinate in Fig. 3) and is chosen for each year such that Eq. (2) is satisﬁed. hr refers to the
        # ANNUALLY UPDATED glacier extent."
        self.scaling_factor_mm = delta_mass_balance_mm / (np.sum(self.areas_perc[increment] * self.delta_water_equivalents_o))
        
    def update_glacier_thickness(self, increment):
        """
        Step 3 of Seibert et al. (2018):
        "For each elevation band reduce the glacier water equivalent according
        to the empirical functions from Huss et al. (2010) (Eq. 4) to
        compute the glacier geometry for the reduced mass (see Eq. 6). If
        the computed thickness change is larger than the remaining glacier
        thickness (most likely to occur at the glacier tongue; see the area
        that is marked in red in the ﬁgure), the glacier thickness is reduced to
        zero, resulting in a glacier-free elevation band, and the portion of the
        glacier thickness change that would have resulted in a negative glacier
        thickness is included in the next iteration step (i.e. the next 1% melt)."
        """
        
        ### Update glacier thicknesses (Eq. 6)
        # Glacier water equivalent reduction
        gwe_reduction_mm = self.scaling_factor_mm * self.delta_water_equivalents_o
        reduced_thickness_mm = self.water_equivalents_mm[increment] - gwe_reduction_mm
        
        self.water_equivalents_mm[increment + 1] = np.where(reduced_thickness_mm < 0, 
                                                            0, reduced_thickness_mm)
        
        self.excess_melt_mass_mm = - np.sum(np.where(reduced_thickness_mm < 0, 
                                                     reduced_thickness_mm, 0)
                                                     * self.areas_perc[increment])
        # This excess melt is taken into account in Step 2.
        
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
        self.areas_perc[increment + 1] = np.where(self.water_equivalents_mm[increment + 1] == 0, 
                                                         0, self.areas_perc[increment])
        
        
        
    def width_scaling(self):
        """
        Step 4 of Seibert et al. (2018):
        "The delta h approach distributes the change in glacier mass over the
        different elevation zones though it results in glacier-free areas mainly
        at the lowest elevations. The width scaling within each elevation band
        relates a decrease in glacier thickness to a reduction of the glacier area
        within the respective elevation band. In other words, this approach also
        allows for glacier area shrinkage at higher elevations, which mimics
        the typical spatial effect of the downwasting of glaciers."
        """
        
        ### Width scaling (Eq. 7)
        for increment in range(len(self.water_equivalents_mm)):
            self.scaled_areas_perc[increment] = self.initial_areas_perc * np.power(self.water_equivalents_mm[increment] / self.water_equivalents_mm[0], 0.5)
            # If we want to keep the same glacial volume, we need to modify accordingly the thickness
            self.scaled_water_equivalents_mm[increment] = self.areas_perc[increment] * self.water_equivalents_mm[increment] / self.scaled_areas_perc[increment]
        
    def compute_fractions_per_elevation_zones(self):
        """
        Step 5 of Seibert et al. (2018):
        "Deﬁne elevation zones and compute the fractions of glacier and non-glacier
        area (relative to the catchment area) for each elevation zone."
        """
        
        ### Deﬁne elevation zones - Not done here as I suppose they will be defined by us beforehand for Hydrobricks (HRUs)
        #------
        
        ### Compute the fraction of glacier and non-glacier areas.
        # fractions = NOT SURE I GET IT...
        return 
        
    def update_elevation_zones_areas(self, increment):
        """
        Step 6 of Seibert et al. (2018):
        "Sum the total (width-scaled) areas for all respective elevation bands
        which are covered by glaciers (i.e. glacier water equivalent ≥ 0) for
        each elevation zone."
        """
        
        ### Update elevation zone areas
        for i, elevation_zone in enumerate(self.elevation_zones): # Because the elevation zones do not necessarily start with 0.
            indices = np.where(self.elevation_zone_id == elevation_zone)[0]
            if len(indices) != 0:
                self.elevation_zone_areas_m2[increment + 1, i] = np.sum(self.areas_perc[increment + 1][indices]) * self.catchment_area_m2
            else:
                self.elevation_zone_areas_m2[increment + 1, i] = 0
        
    def write_lookup_table(self):
        """
        Step 7 of Seibert et al. (2018):
        "M (in % of initial M) is in the ﬁrst column, followed by one column
        for each elevation zone with the areal glacier cover area (in % of
        catchment area)."
        """
        
        ### Write record to the lookup table
        lookup_table = pd.DataFrame(self.elevation_zone_areas_m2, 
                                    index=range(self.nb_increments), 
                                    columns=range(len(self.elevation_zones)))
        lookup_table.to_csv("lookup_table.csv")
        
        scaled_areas_lookup_table = pd.DataFrame(self.scaled_areas_perc * self.catchment_area_m2, 
                                    index=range(self.nb_increments), 
                                    columns=range(len(self.scaled_areas_perc[0])))
        scaled_areas_lookup_table.to_csv("scaled_areas_lookup_table.csv")
        
        areas_lookup_table = pd.DataFrame(self.areas_perc * self.catchment_area_m2, 
                                          index=range(self.nb_increments), 
                                          columns=range(len(self.areas_perc[0])))
        areas_lookup_table.to_csv("areas_lookup_table.csv")
        
        scaled_we_lookup_table = pd.DataFrame(self.scaled_water_equivalents_mm / 1000, 
                                    index=range(self.nb_increments), 
                                    columns=range(len(self.scaled_water_equivalents_mm[0])))
        scaled_we_lookup_table.to_csv("scaled_we_lookup_table.csv")
        
        we_lookup_table = pd.DataFrame(self.water_equivalents_mm / 1000, 
                                    index=range(self.nb_increments), 
                                    columns=range(len(self.water_equivalents_mm[0])))
        we_lookup_table.to_csv("we_lookup_table.csv")
        
    def loop_through_the_steps(self):
        """
        Seibert et al. (2018):
        "Melt the glacier in steps of 1 % of its total mass"
        """
        
        self.compute_initial_state()
        
        for increment in range(self.nb_increments - 1):
            self.calculate_total_glacier_mass(increment)
            self.update_glacier_thickness(increment) # THIS CAN PROBABLY DONE MORE EFFICIENTLY AS WE USE PYTHON
            self.width_updating(increment)
            self.compute_fractions_per_elevation_zones() # I REMOVED THE DEFINITION OF THE ELEVATION ZONES FROM HERE
            self.update_elevation_zones_areas(increment) # UNCLEAR # AND FROM WHAT I UNDERSTAND, NOT SURE WHY IT SHOULD BE IN THE LOOP...
        self.width_scaling()
        self.write_lookup_table() # FOR SURE, THIS SHOULDN'T BE IN THE LOOP
        
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
            trick_to_plot = np.append(scaled_areas_lookup_table.iloc[i, :].values[0], scaled_areas_lookup_table.iloc[i, :].values)
            plt.plot(trick_to_plot, all_bands, drawstyle="steps-post", color="lightgrey")
    # Black lines
    for i in range(len(scaled_areas_lookup_table)):
        if i % 20 == 0:
            trick_to_plot = np.append(scaled_areas_lookup_table.iloc[i, :].values[0], scaled_areas_lookup_table.iloc[i, :].values)
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
            trick_to_plot = np.append(scaled_areas_lookup_table.iloc[i, :].values[0], scaled_areas_lookup_table.iloc[i, :].values)
            plt.plot(trick_to_plot/areas_df["Glacier area [m²]"][8:-7], all_bands, drawstyle="steps-post", color="lightgrey")
    # Black lines
    for i in range(len(scaled_areas_lookup_table)):
        if i % 20 == 0:
            trick_to_plot = np.append(scaled_areas_lookup_table.iloc[i, :].values[0], scaled_areas_lookup_table.iloc[i, :].values)
            plt.plot(trick_to_plot/areas_df["Glacier area [m²]"][8:-7], all_bands, drawstyle="steps-post", color="black")
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
    plt.plot(volumes_df["Glacier volume [m³]"]*1000, [x + 5 for x in volumes_df["Elevation [m a.s.l.]"]], 
             drawstyle="steps-post", color='black')
    plt.plot(thicknesses_df["Glacier thickness [m]"]*areas_df["Glacier area [m²]"], [x + 5 for x in areas_df["Elevation [m a.s.l.]"]], 
             drawstyle="steps-post", color='red')
    plt.xlabel('Glacier volume (m³)')
    plt.ylabel('Elevation (m a.s.l.)')
    
    
    plt.figure()
    # Grey lines
    for i in range(len(areas_lookup_table)):
        if i % 5 == 0:
            trick_to_plot = np.append(areas_lookup_table.iloc[i, :].values[0] * we_lookup_table.iloc[i, :].values[0] / WATER_EQ, 
                                      areas_lookup_table.iloc[i, :].values * we_lookup_table.iloc[i, :].values / WATER_EQ)
            plt.plot(trick_to_plot, all_bands, drawstyle="steps-post", color="lightgrey")
    # Black lines
    for i in range(len(areas_lookup_table)):
        if i % 20 == 0:
            trick_to_plot = np.append(areas_lookup_table.iloc[i, :].values[0] * we_lookup_table.iloc[i, :].values[0] / WATER_EQ, 
                                      areas_lookup_table.iloc[i, :].values * we_lookup_table.iloc[i, :].values / WATER_EQ)
            plt.plot(trick_to_plot, all_bands, drawstyle="steps-post", color="black")
    plt.plot(thicknesses_df["Glacier thickness [m]"] * areas_df["Glacier area [m²]"], [x + 5 for x in areas_df["Elevation [m a.s.l.]"]], 
             drawstyle="steps-post", color='red')
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
    length = int(len(areas_df["Elevation [m a.s.l.]"].values) / 4) # For steps of 10 m, and zones of 40 m.
    ids = [i for i in range(length) for _ in range(4)]
    catchment_area_m2 = 100000000
    
    water_equivalents_mm = thicknesses_df["Glacier thickness [m]"].values * WATER_EQ * 1000 # water equivalent conversion + m to mm conversion
    
    glacier_data = {"elevation_bands_m": areas_df["Elevation [m a.s.l.]"].values,            # Median elevation for each elevation band i. An elevation band is smaller than a HRU, usually around 10 m high.
                    "elevation_zone_id": ids,                                                # For each elevation band i, the ID of the corresponding elevation zone (the HRU).
                    "initial_water_equivalents_mm": water_equivalents_mm,                    # Water equivalent in mm (depth) for each elevation band i
                    "initial_areas_m2": areas_df["Glacier area [m²]"].values}                # Glaciated areas for each elevation band i
    
    glacier_df = pd.DataFrame(glacier_data)
    # We have to remove the bands with no glacier area (otherwise the min and max elevations are wrong)
    glacier_df = glacier_df.drop(glacier_df[glacier_df.initial_areas_m2 == 0].index)
    elevation_zones = np.unique(glacier_df.elevation_zone_id)
    
    nb_increments = 100 # This means that the lookup table consists of glacier areas per elevation
    # zone for 101 different glacier mass situations, ranging from the initial glacier mass to zero.
    
    glacier = GlacierMassBalance(glacier_df, elevation_zones, nb_increments, catchment_area_m2)
    glacier.loop_through_the_steps()
    
    plot_figure_2b(elevation_bands=glacier_df.elevation_bands_m.values)
    print("Terminated")
    
main()
    