from abc import ABC, abstractmethod

from hydrobricks._exceptions import ConfigurationError


class Action(ABC):
    """Base abstract class for model actions (dynamic updates to model state)."""

    @abstractmethod
    def __init__(self) -> None:
        """
        Initialize the Action base class.

        Subclasses should call super().__init__() and set appropriate
        name and is_initialized values.
        """
        self.is_initialized: bool = False
        self.name: str = "BaseAction"

    @staticmethod
    def _convert_month_to_number(update_month: str | int) -> int:
        """
        Convert a month name or number to a month number (1-12).

        Parameters
        ----------
        update_month
            The month to convert. Can be:
            - Full English month name (e.g., 'January', 'December')
            - Integer from 1 to 12

        Returns
        -------
        int
            Month as an integer (1-12).

        Raises
        ------
        ConfigurationError
            If month number is not between 1 and 12, or if month name is not recognized,
            or if update_month is neither a string nor an integer.

        Examples
        --------
        >>> Action._convert_month_to_number('January')
        1
        >>> Action._convert_month_to_number(12)
        12
        """
        # Convert the month name to a number
        if isinstance(update_month, int):
            if update_month < 1 or update_month > 12:
                raise ConfigurationError(
                    "Month number must be between 1 and 12.",
                    item_name="update_month",
                    item_value=update_month,
                    reason="Invalid month number",
                )
            month_num = update_month
        elif isinstance(update_month, str):
            month_mapping = {
                "January": 1,
                "February": 2,
                "March": 3,
                "April": 4,
                "May": 5,
                "June": 6,
                "July": 7,
                "August": 8,
                "September": 9,
                "October": 10,
                "November": 11,
                "December": 12,
            }
            month_num = month_mapping.get(update_month, None)
            if month_num is None:
                raise ConfigurationError(
                    f"Invalid month name: {update_month}",
                    item_name="update_month",
                    item_value=update_month,
                    reason="Unknown month name",
                )
        else:
            raise ConfigurationError(
                "Month must be a string or an integer.",
                item_name="update_month",
                item_value=type(update_month).__name__,
                reason="Invalid type",
            )

        return month_num
