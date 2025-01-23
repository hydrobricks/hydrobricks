import numpy as np

class GlacierMassBalance:
    """Class for glacier mass balance"""
    
    def __init__(self, nb_elevation_bands, nb_increments):
        
        # Step 1
        self.areas = np.nan
        self.total_glacier_mass = np.nan
        self.normalized_elevations = np.nan
        
        # Step 2
        self.delta_water_equivalents = np.nan
        self.scaling_factor = np.nan
        # Delta-H parametrization based on Huss et al. (2010).
        self.a_coef = np.nan
        self.b_coef = np.nan
        self.c_coef = np.nan
        self.gamma_coef = np.nan
        
        # water_equivalents: water equivalent in mm (depth) for each elevation band i,
        # and for each increment.
        self.water_equivalents = np.ones((nb_increments, nb_elevation_bands))
        self.nb_increments = nb_increments

    def compute_initial_state(self, water_equivalents, elevations, areas):
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
        
        ### Define glacier profile
        self.areas = areas
        self.water_equivalents[0] = water_equivalents
        
        ### Calculate total glacier mass (Eq. 2)
        # areas: area (expressed as a proportion of the catchment area) for each elevation band i
        self.total_glacier_mass = np.sum(self.areas * self.water_equivalents[0])
        
        ### Normalize glacier elevations (Eq. 3)
        # elevations: absolute elevation for each elevation band i
        max_elevation = np.max(elevations)
        min_elevation = np.min(elevations)
        self.normalized_elevations = (max_elevation - elevations) / (max_elevation - min_elevation)
        
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
        
    def calculate_total_glacier_mass(self, glacier_area_km2):
        """
        Step 2 of Seibert et al. (2018):
        "Depending on the glacier area, select one of the three parameterizations
        suggested by Huss et al. (2010)."
        
        delta_mass_balance: the glacier volume change increment
        """
        
        ### Choose the right glacial parametrization (Huss et al., 2010, Fig. 2a)
        glacier_area_km2 = np.sum(self.areas) / 1000 # OR NOT? ARE WE TALKING ABOUT GLACIATED AREAS OR HRU AREAS???
        self.parametrization(glacier_area_km2)
        
        ### Apply ∆h-parameterization (Eq. 4)
        # gives the normalized (dimensionless) ice thickness change for each elevation band i
        self.delta_water_equivalents = np.power(self.normalized_elevations + self.a_coef, self.gamma_coef) + \
                                        self.b_coef * (self.normalized_elevations + self.a_coef) + self.c_coef
        
        ### Calculate the scaling factor fs (Eq. 5)
        delta_mass_balance = self.total_glacier_mass / 100
        print(self.areas)
        print(self.delta_water_equivalents)
        print(self.areas * self.delta_water_equivalents)
        self.scaling_factor = delta_mass_balance / np.sum(self.areas * self.delta_water_equivalents)
        
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
        self.water_equivalents[increment + 1] = self.water_equivalents[increment] + self.scaling_factor * self.delta_water_equivalents
        
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
        # TO CHECK, REALLY
        scaled_areas = self.areas * np.power(self.water_equivalents / self.water_equivalents[0], 0.5)
        
    def define_elevation_zones(self):
        """
        Step 5 of Seibert et al. (2018):
        "Deﬁne elevation zones and compute the fractions of glacier and non-glacier
        area (relative to the catchment area) for each elevation zone."
        """
        
        ### Deﬁne elevation zones
        
    def update_elevation_zones_areas(self):
        """
        Step 6 of Seibert et al. (2018):
        "Sum the total (width-scaled) areas for all respective elevation bands
        which are covered by glaciers (i.e. glacier water equivalent ≥ 0) for
        each elevation zone."
        """
        
        ### Update elevation zone areas
        
    def write_lookup_table(self):
        """
        Step 7 of Seibert et al. (2018):
        "M (in % of initial M) is in the ﬁrst column, followed by one column
        for each elevation zone with the areal glacier cover area (in % of
        catchment area)."
        """
        
        ### Write record to the lookup table
        
    def loop_through_the_steps(self, initial_water_equivalents, elevations, areas, glacier_area_km2):
        """
        Seibert et al. (2018):
        "Melt the glacier in steps of 1 % of its total mass"
        """
        
        self.compute_initial_state(initial_water_equivalents, elevations, areas)
        self.define_elevation_zones() # UNCLEAR
        self.calculate_total_glacier_mass(glacier_area_km2)
        for increment in range(self.nb_increments - 1): # CHECK THE RANGE # + I DO NOT UNDERSTAND WHY THE STEP 2 SHOULD BE IN THE LOOP??!
            self.update_glacier_thickness(increment) # THIS CAN PROBABLY DONE MORE EFFICIENTLY AS WE USE PYTHON
            self.scaled_areas = self.width_scaling()
            self.update_elevation_zones_areas() # UNCLEAR
            self.write_lookup_table()
        
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
        
def main():
    print("Here")
    nb_elevation_bands = 4
    nb_increments = 100 # CHECK IF IT IS FIXED?
    elevation_bands = [2100, 2110, 2170, 2180] # Median of the elevation bands (smaller than the HRU)
    initial_water_equivalents = [60, 50, 80, 70] # Water equivalent in mm (depth) for each elevation band i
    areas = [20, 20, 20, 20]
    glacier_area_km2 = np.sum(areas)
    
    glacier = GlacierMassBalance(nb_elevation_bands, nb_increments)
    glacier.loop_through_the_steps(initial_water_equivalents, elevation_bands, areas, glacier_area_km2)
    print("Terminated")
    
main()
    