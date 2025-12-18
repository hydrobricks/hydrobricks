import math

# Radian to Degree and Degree to Radian multiplicative factors
TO_RAD = math.pi / 180.0
TO_DEG = 180.0 / math.pi

# The orbit of Earth around the sun is an ellipse with an eccentricity of 0.017
# and a semi-major axis of 149,598,023 km.
ES_SM_AXIS = 149.6  # Earth-Sun radius semi-major axis (i.e. mean Sun-Earth distance)
ES_ECCENTRICITY = 0.017  # Eccentricity
SOLAR_CST = 1368  # Solar constant [W/m²]

# To compute the atmospheric pressure
SEA_ATM_PRESSURE = 101325  # Atmospheric pressure at sea-level, standard atmosphere [Pa]
SEA_HEIGHT = 0  # Height at sea-level, at bottom of atmospheric layer [m]
SEA_SURFACE_TEMPERATURE = 288  # Temperature at sea-level [K] (15 deg C)
T_LAPSE_RATE = -0.0065  # Standard temperature lapse rate (-0.0065) [K/m]
GRAVITY = 9.80665  # Gravitational acceleration [m/s²]
R_GAS = 8.31432  # Universal gas constant [J/(mol·K)]
AIR_MOLAR_MASS = 0.0289644  # Molar mass of Earth's air [kg/mol]

# For the glacier melt
ICE_DENSITY = 900  # Ice density [kg/m³]
WATER_DENSITY = 1000  # Water density [kg/m³]
ICE_WE = ICE_DENSITY / WATER_DENSITY  # Water equivalent [-]
