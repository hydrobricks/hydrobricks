"""Soil parameter tables shared across the models.

Soil maps often come as class codes rather than as physical values: a map tells that
a cell belongs to field-capacity class 3, and a table says what that class is worth.
The tables below are such class-to-value mappings, named after the source they were
transcribed from, and are meant for the ``reclassify`` argument of
:meth:`~hydrobricks.catchment.Catchment.extract_unit_property_from_raster` and
:meth:`~hydrobricks.catchment.Catchment.extract_soil_properties`.
"""

#: Available water content [Vol-%] of each class of the Swiss soil suitability map
#: (the ``NFK`` grid), as used by PREVAH (manual part II, p. 19). It feeds the soil
#: moisture capacity through
#: :meth:`~hydrobricks.models.PrevahUniBE.land_use_field_capacity`.
PREVAH_SOIL_AWC_CLASSES: dict[int, float] = {
    1: 1.5,
    2: 4.5,
    3: 8.0,
    4: 11.0,
    5: 16.0,
    6: 24.0,
}

#: Soil depth [m] of each class of the Swiss soil suitability map (the ``BTIF``
#: grid), as used by PREVAH. It caps the rooting depth in the soil moisture capacity.
PREVAH_SOIL_DEPTH_CLASSES: dict[int, float] = {
    1: 0.1,
    2: 0.25,
    3: 0.5,
    4: 1.0,
    5: 2.0,
    6: 4.0,
}

#: Hydraulic conductivity of each class of the Swiss soil suitability map (the ``K``
#: grid), as used by PREVAH. The values span seven orders of magnitude; only their
#: logs relative to the catchment matter, through
#: :meth:`~hydrobricks.models.PrevahUniBE.conductivity_factor` (PREVAH's KWPER).
PREVAH_SOIL_CONDUCTIVITY_CLASSES: dict[int, float] = {
    1: 0.005,
    2: 0.05,
    3: 0.5,
    4: 5.0,
    5: 50.0,
    6: 500.0,
    7: 5000.0,
}
