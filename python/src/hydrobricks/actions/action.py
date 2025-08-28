from abc import ABC, abstractmethod


class Action(ABC):
    """Base class for the actions"""

    @abstractmethod
    def __init__(self):
        pass

    @staticmethod
    def _convert_month_to_number(update_month: str | int) -> int:
        """
        Convert the month name to a number.

        Parameters
        ----------
        update_month
            The month to convert. Full english name or number (1-12).

        Returns
        -------
        The month as a number (1-12).
        """
        # Convert the month name to a number
        if isinstance(update_month, int):
            if update_month < 1 or update_month > 12:
                raise ValueError("Month number must be between 1 and 12.")
            month_num = update_month
        elif isinstance(update_month, str):
            month_mapping = {
                'January': 1,
                'February': 2,
                'March': 3,
                'April': 4,
                'May': 5,
                'June': 6,
                'July': 7,
                'August': 8,
                'September': 9,
                'October': 10,
                'November': 11,
                'December': 12
            }
            month_num = month_mapping.get(update_month, None)
            if month_num is None:
                raise ValueError(f"Invalid month name: {update_month}")
        else:
            raise ValueError("Month must be a string or an integer.")

        return month_num
