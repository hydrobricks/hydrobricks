from __future__ import annotations

from hydrobricks._hydrobricks import (
    ActionGlacierSnowToIceTransformation as _ActionGlacierSnowToIceTransformation,
)
from hydrobricks.actions import Action


class ActionGlacierSnowToIceTransformation(Action):
    """
    Class for the glacier snow to ice transformation action.

    This action transforms all remaining snow on glaciers into ice at a given date,
    typically at the end of the accumulation season.
    """

    def __init__(
            self,
            update_month: str | int = 'September',
            update_day: int = 30,
            land_cover: str = 'glacier',
    ) -> None:
        """
        Initialize the glacier snow to ice transformation action.

        Parameters
        ----------
        update_month
            The month to apply the changes. Full English name or number (1-12).
            The update will be applied at the given date, every year.
            Default: 'September'
        update_day
            The day of the month to apply the changes (1-31). Default: 30
        land_cover
            The land cover name to apply the changes. Default: 'glacier'

        Raises
        ------
        ValueError
            If update_month is invalid or update_day is out of range.
        """
        super().__init__()
        self.name: str = "ActionGlacierSnowToIceTransformation"
        self._create_bounded_instance(
            update_month,
            update_day,
            land_cover
        )

    def get_month(self) -> int:
        """
        Get the month to apply the changes.

        Returns
        -------
        int
            The month number (1-12) when changes are applied.
        """
        return self.action.get_month()

    def get_day(self) -> int:
        """
        Get the day of the month to apply the changes.

        Returns
        -------
        int
            The day of the month (1-31) when changes are applied.
        """
        return self.action.get_day()

    def get_land_cover_name(self) -> str:
        """
        Get the land cover name (glacier name) to apply the changes.

        Returns
        -------
        str
            The land cover name (glacier name) where changes are applied.
        """
        return self.action.get_land_cover_name()

    def _create_bounded_instance(
            self,
            update_month: str | int,
            update_day: int,
            land_cover: str,
    ) -> None:
        """
        Create the internal C++ action instance.

        Parameters
        ----------
        update_month
            The month (name or number) to apply changes.
        update_day
            The day of the month to apply changes.
        land_cover
            The land cover name to apply changes to.

        Raises
        ------
        ValueError
            If month conversion fails or parameters are invalid.
        """
        month_num = self._convert_month_to_number(update_month)
        self.action: _ActionGlacierSnowToIceTransformation = (
            _ActionGlacierSnowToIceTransformation(month_num, update_day, land_cover)
        )

        self.is_initialized = True
