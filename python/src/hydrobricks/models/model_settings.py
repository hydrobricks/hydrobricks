from _hydrobricks import SettingsModel


class ModelSettings:
    """Base class for the model settings"""

    def __init__(
            self,
            solver: str = 'heun_explicit',
            record_all: bool = False,
            **kwargs: dict
    ):
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
        self.settings = SettingsModel()
        self.settings.log_all(record_all)
        self.settings.set_solver(solver)

    def set_timer(
            self,
            start_date: str,
            end_date: str,
            time_step: int = 1,
            time_step_unit: str = 'day'
    ):
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

    def set_parameter_value(
            self,
            component: str,
            name: str,
            value: float
    ) -> bool:
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

    def generate_base_structure(
            self,
            land_cover_names: list[str],
            land_cover_types: list[str],
            with_snow: bool = True,
            snow_melt_process: str = 'melt:degree_day',
            snow_ice_transformation: str = None,
            snow_redistribution: str = None
    ):
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
        snow_ice_transformation
            Snow and ice transformation method (optional)
        snow_redistribution
            Snow redistribution method (optional)
        """
        if len(land_cover_names) != len(land_cover_types):
            raise RuntimeError('The length of the land cover names '
                               'and types do not match.')

        # Precipitation
        self.settings.generate_precipitation_splitters(with_snow)

        # Add default ground land cover
        self.settings.add_land_cover_brick('ground', 'generic_land_cover')

        # Add other specific land covers
        for cover_type, cover_name in zip(land_cover_types, land_cover_names):
            if cover_type not in ['ground', 'generic_land_cover']:
                self.settings.add_land_cover_brick(cover_name, cover_type)

        # Snowpack
        if with_snow:
            self.settings.generate_snowpacks(snow_melt_process)
            if snow_ice_transformation:
                self.settings.add_snow_ice_transformation(snow_ice_transformation)
            if snow_redistribution:
                self.settings.add_snow_redistribution(snow_redistribution)

    def add_land_cover_brick(self, name: str, kind: str):
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

    def add_hydro_unit_brick(self, name: str, kind: str):
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

    def add_sub_basin_brick(self, name: str, kind: str):
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

    def select_hydro_unit_brick(self, name: str):
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
            target: str = '',
            log: bool = False,
            instantaneous: bool = False
    ):
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
        if kind in ['outflow:direct', 'outflow:rest_direct']:
            self.settings.set_process_outputs_as_static()

        # Define output as instantaneous
        if instantaneous:
            self.settings.set_process_outputs_as_instantaneous()

    def add_brick_parameter(
            self,
            name: str,
            value: int | float | bool,
            kind: str = 'constant'
    ):
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

    def add_process_parameter(
            self,
            name: str,
            value: int | float | bool,
            kind: str = 'constant'
    ):
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

    def add_logging_to(self, item: str):
        """
        Add logging to an item

        Parameters
        ----------
        item
            Name of the item
        """
        self.settings.add_logging_to(item)

    def set_process_outputs_as_instantaneous(self):
        """Set all process outputs as instantaneous"""
        self.settings.set_process_outputs_as_instantaneous()

    def set_process_outputs_as_static(self):
        """Set all process outputs as static"""
        self.settings.set_process_outputs_as_static()
