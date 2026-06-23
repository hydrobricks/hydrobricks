from typing import Any

from hydrobricks._exceptions import ConfigurationError
from hydrobricks._hydrobricks import SettingsModel


class ModelSettings:
    """Base class for the model settings"""

    def __init__(
        self, solver: str = "heun_explicit", record_all: bool = False, **kwargs: Any
    ) -> None:
        """
        Parameters
        ----------
        solver
            Solver to use
        record_all
            Record all state and flux values
        kwargs
            Keyword arguments
        """
        self.settings: SettingsModel = SettingsModel()
        self.settings.log_all(record_all)
        self.settings.set_solver(solver)

    def set_timer(
        self,
        start_date: str,
        end_date: str,
        time_step: int = 1,
        time_step_unit: str = "day",
    ) -> None:
        """
        Set the timer

        Parameters
        ----------
        start_date
            Start date of the simulation
        end_date
            End date of the simulation
        time_step
            Time step
        time_step_unit
            Time step unit
        """
        self.settings.set_timer(start_date, end_date, int(time_step), time_step_unit)

    def set_parameter_value(self, component: str, name: str, value: float) -> bool:
        """
        Set a parameter value

        Parameters
        ----------
        component
            Name of the component
        name
            Name of the parameter
        value
            Value of the parameter

        Returns
        -------
        True if the parameter was set successfully, False otherwise.
        """
        return self.settings.set_parameter_value(component, name, float(value))

    def get_structure(self) -> list:
        """
        Export the model structure (bricks, processes, fluxes, splitters).

        Returns
        -------
        A list of structure-variant dicts (see SettingsModel.get_structure in the C++
        bindings); used to build the model structure graph.
        """
        return self.settings.get_structure()

    def generate_base_structure(
        self,
        land_cover_names: list[str],
        land_cover_types: list[str],
        with_snow: bool = True,
        snow_melt_process: str = "melt:degree_day",
        snow_rain_process: str | None = None,
        snow_ice_transformation: str | None = None,
        snow_redistribution: str | None = None,
        snow_water_retention_process: str | None = None,
        snow_refreezing_process: str | None = None,
        rain_to_snowpack: bool = False,
        forest_interception: bool = False,
    ) -> None:
        """
        Generate basic elements

        Parameters
        ----------
        land_cover_names
            List of land cover names
        land_cover_types
            List of land cover types
        with_snow
            Account for snow
        snow_melt_process
            Snow melt process
        snow_rain_process
            Rain/snow partitioning method (overrides the
            default derived from snow_melt_process)
        snow_ice_transformation
            Snow and ice transformation method (optional)
        snow_redistribution
            Snow redistribution method (optional)
        snow_water_retention_process
            Outflow process of the snowpack liquid water storage (optional). When
            provided, the snowpacks are generated with liquid water retention: the
            melt water is kept in the snowpack water container and released by the
            given process (e.g. 'outflow:snow_holding').
        snow_refreezing_process
            Refreezing process of the retained liquid water (optional; requires
            snow_water_retention_process). E.g. 'refreeze:degree_day'.
        rain_to_snowpack
            Route the rain to the snowpack liquid water storage instead of the
            land cover (requires snow_water_retention_process).
            The rain is retained in the snowpack (up to the holding capacity)
            and exposed to refreezing; without snow, it reaches the land cover
            within the same time step.
        forest_interception
            Add a canopy interception store on each ``forest`` land cover (default
            False). When False, forest covers behave like a generic soil cover.
        """
        if len(land_cover_names) != len(land_cover_types):
            raise ConfigurationError(
                "The length of the land cover names and types do not match.",
                reason="Mismatched array sizes",
            )

        # Precipitation
        if snow_rain_process is not None:
            splitter_type = snow_rain_process
        elif snow_melt_process == "melt:cemaneige":
            splitter_type = "snow_rain:cemaneige"
        else:
            splitter_type = "snow_rain:linear"
        self.settings.generate_precipitation_splitters(with_snow, splitter_type)

        # Add the land covers, each by its own name: generic-behaviour covers (incl.
        # forest, which is generic plus a canopy) map to the generic_land_cover brick,
        # while special covers (e.g. glacier) keep their type. Several generic covers
        # can coexist (e.g. open and forest), each getting its own snowpack and
        # soil routine.
        for cover_type, cover_name in zip(land_cover_types, land_cover_names):
            if cover_type in ["ground", "generic_land_cover", "open", "forest", "lake"]:
                self.settings.add_land_cover_brick(cover_name, "generic_land_cover")
            else:
                self.settings.add_land_cover_brick(cover_name, cover_type)

        # Forest canopy interception (opt-in), on the rain path upstream of the
        # snowpack. Generated before the snowpacks so the canopy (a surface component)
        # is declared/computed before the snowpack it feeds; the throughfall rejoins the
        # original rain target (the snowpack when the rain is routed to it, otherwise
        # the land cover). When disabled, forest covers behave like a generic soil cover
        # and interception can be accounter for through ET correction.
        if forest_interception:
            rain_to_snowpack_active = with_snow and rain_to_snowpack
            for cover_type, cover_name in zip(land_cover_types, land_cover_names):
                if cover_type == "forest":
                    if rain_to_snowpack_active:
                        throughfall_target = f"{cover_name}_snowpack"
                    else:
                        throughfall_target = cover_name
                    self.settings.generate_canopy_interception(
                        cover_name, throughfall_target
                    )

        # Snowpack
        if with_snow:
            if snow_refreezing_process and not snow_water_retention_process:
                raise ConfigurationError(
                    "Snow refreezing requires a snow water retention process.",
                    item_name="snow_refreezing_process",
                    item_value=snow_refreezing_process,
                    reason="Missing snow water retention process",
                )
            if rain_to_snowpack and not snow_water_retention_process:
                raise ConfigurationError(
                    "Routing the rain to the snowpacks requires a snow water "
                    "retention process.",
                    item_name="rain_to_snowpack",
                    item_value=rain_to_snowpack,
                    reason="Missing snow water retention process",
                )
            if snow_water_retention_process:
                self.settings.generate_snowpacks_with_water_retention(
                    snow_melt_process,
                    snow_water_retention_process,
                    rain_to_snowpack,
                )
                if snow_refreezing_process:
                    self.settings.add_snowpack_refreezing(snow_refreezing_process)
            else:
                self.settings.generate_snowpacks(snow_melt_process)
            if snow_ice_transformation:
                self.settings.add_snow_ice_transformation(snow_ice_transformation)
            if snow_redistribution:
                self.settings.add_snow_redistribution(snow_redistribution)

    def add_land_cover_brick(self, name: str, kind: str) -> None:
        """
        Add a land cover brick

        Parameters
        ----------
        name
            Name of the land cover brick
        kind
            Type of the land cover brick
        """
        self.settings.add_land_cover_brick(name, kind)

    def add_hydro_unit_brick(self, name: str, kind: str) -> None:
        """
        Add a hydro unit brick

        Parameters
        ----------
        name
            Name of the hydro unit brick
        kind
            Type of the hydro unit brick
        """
        self.settings.add_hydro_unit_brick(name, kind)

    def add_sub_basin_brick(self, name: str, kind: str) -> None:
        """
        Add a sub basin brick

        Parameters
        ----------
        name
            Name of the sub basin brick
        kind
            Type of the sub basin brick
        """
        self.settings.add_sub_basin_brick(name, kind)

    def select_hydro_unit_brick(self, name: str) -> None:
        """
        Select a hydro unit brick

        Parameters
        ----------
        name
            Name of the hydro unit brick
        """
        self.settings.select_hydro_unit_brick(name)

    def add_brick_process(
        self,
        name: str,
        kind: str,
        target: str = "",
        log: bool = False,
        instantaneous: bool = False,
    ) -> None:
        """
        Add a brick process

        Parameters
        ----------
        name
            Name of the brick process
        kind
            Type of the brick process
        target
            Target of the process output
        log
            Log the brick process
        instantaneous
            Process outputs are instantaneous
        """
        self.settings.add_brick_process(name, kind, target, log)

        # Define output as static
        if kind in ["outflow:direct", "outflow:rest"]:
            self.settings.set_process_outputs_as_static()

        # Define output as instantaneous
        if instantaneous:
            self.settings.set_process_outputs_as_instantaneous()

    def add_process_output(self, target: str) -> None:
        """
        Add an extra output target to the most recently added process.

        Parameters
        ----------
        target
            Target brick of the additional output.
        """
        self.settings.add_process_output(target)

    def add_brick_parameter(
        self, name: str, value: int | float | bool, kind: str = "constant"
    ) -> None:
        """
        Add a brick parameter

        Parameters
        ----------
        name
            Name of the brick parameter
        value
            Value of the brick parameter
        kind
            Type of the brick parameter (for now has to be 'constant')
        """
        self.settings.add_brick_parameter(name, float(value), kind)

    def set_current_brick_computed_directly(self) -> None:
        """
        Mark the selected brick as computed directly (explicitly, without the ODE
        solver). Used for fully explicit formulations such as the GR4J production
        store and routing, where processes apply an exact discrete update each step.
        """
        self.settings.set_current_brick_computed_directly()

    def add_process_parameter(
        self, name: str, value: int | float | bool, kind: str = "constant"
    ) -> None:
        """
        Add a process parameter

        Parameters
        ----------
        name
            Name of the process parameter
        value
            Value of the process parameter
        kind
            Type of the process parameter (for now has to be 'constant')
        """
        self.settings.add_process_parameter(name, float(value), kind)

    def add_logging_to(self, item: str) -> None:
        """
        Add logging to an item

        Parameters
        ----------
        item
            Name of the item
        """
        self.settings.add_logging_to(item)

    def add_structure(self) -> int:
        """
        Add a new (empty) model-structure variant and select it.

        Subsequent structure-building calls populate the newly selected structure.
        Units are auto-assigned (in the core) to the variant matching their land
        covers.

        Returns
        -------
        The id of the newly created structure.
        """
        return self.settings.add_structure()

    def set_process_outputs_as_instantaneous(self) -> None:
        """Set all process outputs as instantaneous"""
        self.settings.set_process_outputs_as_instantaneous()

    def set_process_outputs_as_static(self) -> None:
        """Set all process outputs as static"""
        self.settings.set_process_outputs_as_static()
