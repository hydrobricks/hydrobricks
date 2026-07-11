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
- discharge.csv     : daily reference discharge (mm/d). This is the *Fortran
                      PREVAH* simulated total runoff (out.xdyc r_tot), not a
                      gauge observation; the example reproduces Fortran PREVAH.

Provenance / licensing
----------------------
Derived from the WSL xprevah Ticino-Bellinzona test case. The meteorological
forcing originates from MeteoSwiss RhiresD/TabsD gridded products.
