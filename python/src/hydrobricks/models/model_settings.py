from _hydrobricks import SettingsModel


class ModelSettings:
    """Base class for the model settings"""

    def __init__(self, solver='heun_explicit', record_all=False, **kwargs):
        """
        Parameters
        ----------
        solver : str
            Solver to use
        record_all : bool
            Record all state and flux values
        kwargs : dict
            Keyword arguments
        """
        self.settings = SettingsModel()

    def set_timer(self, start_date, end_date, time_step=1, time_step_unit='day'):
        """
        Set the timer

        Parameters
        ----------
        start_date : str
            Start date of the simulation
        end_date : str
            End date of the simulation
        time_step : float
            Time step
        time_step_unit : str
            Time step unit
        """
        self.settings.set_timer(start_date, end_date, time_step, time_step_unit)

    def set_parameter(self, component, name, value):
        """
        Set a parameter

        Parameters
        ----------
        component : str
            Name of the component
        name : str
            Name of the parameter
        value : float
            Value of the parameter
        """
        self.settings.set_parameter(component, name, value)

    def generate_base_structure(self, land_cover_names, land_cover_types,
                                snow_melt_process='melt:degree_day'):
        """
        Generate basic elements

        Parameters
        ----------
        land_cover_names : list
            List of land cover names
        land_cover_types : list
            List of land cover types
        snow_melt_process : str
            Snow melt process
        """
        if len(land_cover_names) != len(land_cover_types):
            raise RuntimeError('The length of the land cover names '
                               'and types do not match.')

        # Precipitation
        self.settings.generate_precipitation_splitters(True)

        # Add default ground land cover
        self.settings.add_land_cover_brick('ground', 'generic_land_cover')

        # Add other specific land covers
        for cover_type, cover_name in zip(land_cover_types, land_cover_names):
            self.settings.add_land_cover_brick(cover_name, cover_type)

        # Snowpack
        self.settings.generate_snowpacks(snow_melt_process)

    def generate_snowpacks(self, snow_melt_process):
        """
        Generate snowpacks

        Parameters
        ----------
        snow_melt_process : str
            Snow melt process
        """
        self.settings.generate_snowpacks(snow_melt_process)

    def add_land_cover_brick(self, name, kind):
        """
        Add a land cover brick

        Parameters
        ----------
        name : str
            Name of the land cover brick
        kind : str
            Type of the land cover brick
        """
        self.settings.add_land_cover_brick(name, kind)

    def add_hydro_unit_brick(self, name, kind):
        """
        Add a hydro unit brick

        Parameters
        ----------
        name : str
            Name of the hydro unit brick
        kind : str
            Type of the hydro unit brick
        """
        self.settings.add_hydro_unit_brick(name, kind)

    def add_sub_basin_brick(self, name, kind):
        """
        Add a sub basin brick

        Parameters
        ----------
        name : str
            Name of the sub basin brick
        kind : str
            Type of the sub basin brick
        """
        self.settings.add_sub_basin_brick(name, kind)

    def select_hydro_unit_brick(self, name):
        """
        Select a hydro unit brick

        Parameters
        ----------
        name : str
            Name of the hydro unit brick
        """
        self.settings.select_hydro_unit_brick(name)

    def add_brick_process(self, name, kind, target='', log=False):
        """
        Add a brick process

        Parameters
        ----------
        name : str
            Name of the brick process
        kind : str
            Type of the brick process
        target : str
            Target of the process output
        log : bool
            Log the brick process
        """
        self.settings.add_brick_process(name, kind, target, log)

        if kind.startswith('et:'):
            self.settings.add_process_forcing('pet')
        elif kind == 'outflow:linear':
            self.settings.add_process_parameter('response_factor', 0.2)
        elif kind == 'outflow:constant':
            if name == 'percolation':
                self.settings.add_process_parameter('percolation_rate', 0.1)
        elif kind == 'runoff:socont':
            self.settings.add_process_parameter('runoff_coefficient', 500)
        elif kind == 'melt:degree_day':
            self.settings.add_process_forcing('temperature')
            self.settings.add_process_parameter('degree_day_factor', 3.0)
            self.settings.add_process_parameter('melting_temperature', 0)
        elif kind in ['outflow:direct', 'outflow:rest_direct', 'infiltration:socont',
                      'overflow']:
            pass  # No parameters
        else:
            raise ValueError(f'Unknown brick process: {name}')

    def add_brick_parameter(self, name, value, kind='constant'):
        """
        Add a brick parameter

        Parameters
        ----------
        name : str
            Name of the brick parameter
        value : int|float|bool
            Value of the brick parameter
        kind : str
            Type of the brick parameter (for now has to be 'constant')
        """
        self.settings.add_brick_parameter(name, float(value), kind)

    def add_process_parameter(self, name, value, kind='constant'):
        """
        Add a process parameter

        Parameters
        ----------
        name : str
            Name of the process parameter
        value : int|float|bool
            Value of the process parameter
        kind : str
            Type of the process parameter (for now has to be 'constant')
        """
        self.settings.add_process_parameter(name, float(value), kind)

    def add_logging_to(self, item):
        """
        Add logging to an item

        Parameters
        ----------
        item : str
            Name of the item
        """
        self.settings.add_logging_to_item(item)
