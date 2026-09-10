Ticino-Bellinzona catchment (PREVAH example data)
=================================================

570 hydrotopes (HRUs) of the Ticino-Bellinzona catchment, converted from the
WSL xprevah PREVAH `test/2020` case to hydrobricks-style files.

Files
-----
- hydro_units.csv   : per-HRU elevation, land-cover area (open/forest/wetland),
                      per-HRU field capacity `fc` (mm) and meteo zone `mez`.
- precipitation.csv : daily precipitation (mm/day) per meteo zone (1984-2000).
- temperature.csv   : daily air temperature (deg C) per meteo zone (1984-2000).
- potential_radiation.csv : potential clear-sky direct solar radiation of each
                      HRU, one row per day of the year (1-366), one column per
                      HRU id. Drives the radiation-corrected (Hock) snow melt
                      that the PREVAH control file selects.
- discharge_prevah.csv : daily reference discharge (mm/d). This is the *Fortran
                      PREVAH* simulated total runoff (out.xdyc r_tot), not a
                      gauge observation; the example reproduces Fortran PREVAH.
- discharge.csv     : the gauged discharge of the Ticino at Bellinzona
                      (1981-2020), as volume (m3/s) and specific (mm/d). Shown
                      alongside the two models for reference.
- outline.shp (+ .dbf/.shx/.prj/.qmd) : catchment outline, CH1903+/LV95
                      (EPSG:2056).
- unit_ids.tif      : raster of the HRU ids (500 m, EPSG:2056), for the
                      preprocessing steps that aggregate gridded data onto the
                      hydro units. Converted from the PREVAH hydrotope grid
                      (`2020.idh`), whose coordinates are LV03 (EPSG:21781) and
                      were shifted by +2'000'000 / +1'000'000.

About the potential radiation
-----------------------------
The table is computed the way PREVAH does it (`s_ezenitz`): per hydrotope, from
the latitude and the slope and aspect of that hydrotope, with no terrain
shading. It is deliberately *not* derived from a digital elevation model,
because the calibrated radiation melt factor of the control file (CASNO)
multiplies exactly this quantity. A radiation field computed from a DEM is a
different measure -- it carries the atmospheric attenuation and another
daily-integration convention, and the ratio between the two varies through the
year by about a third -- so it would need its own melt coefficient.

The values are rounded to the nearest integer; at the calibrated CASNO that is
worth about 3e-5 mm/d/degC of melt.

Provenance / licensing
----------------------
Derived from the WSL xprevah Ticino-Bellinzona test case (hydrotope table
`2020.gkw`, hydrotope grid `2020.idh`, control file `cal_2020pest.inp`). The
meteorological forcing originates from MeteoSwiss RhiresD/TabsD gridded
products. The gauged discharge is the federal hydrometric record for the
Ticino at Bellinzona.
