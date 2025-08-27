from __future__ import annotations

import _hydrobricks as _hb
import hydrobricks as hb
from hydrobricks.actions.action import Action


class ActionGlacierSnowToIceTransformation(Action):
    """
    Class for the glacier snow to ice transformation action.

    This action transforms all remaining snow on glaciers into ice at a given date.
    """

    def __init__(
            self,
            update_month: str | int = 'September',
            update_day: int = 30,
            land_cover: str = 'glacier',
    ):
        """
        Initialize the action.

        Parameters
        ----------
        update_month
            The month to apply the changes. Full english name or number (1-12).
            The update will be applied at the given date, every year.
            Default is 'September'.
        update_day
            The day of the month to apply the changes. Default is 30.
        land_cover
            The land cover name to apply the changes. Default is 'glacier'.
        """
        super().__init__()
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
        The month to apply the changes.
        """
        return self.action.get_month()

    def get_day(self) -> int:
        """
        Get the day of the month to apply the changes.

        Returns
        -------
        The day of the month to apply the changes.
        """
        return self.action.get_day()

    def get_land_cover_name(self) -> str:
        """
        Get the land cover name (glacier name) to apply the changes.

        Returns
        -------
        The land cover name (glacier name) to apply the changes.
        """
        return self.action.get_land_cover_name()

    def _create_bounded_instance(
            self,
            update_month: str | int,
            update_day: int,
            land_cover: str,
    ):
        month_num = self._convert_month_to_number(update_month)
        self.action = _hb.ActionGlacierSnowToIceTransformation(
            month_num, update_day, land_cover)
