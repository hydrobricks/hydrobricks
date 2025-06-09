from abc import ABC, abstractmethod


class Action(ABC):
    """Base class for the actions"""

    @abstractmethod
    def __init__(self):
        pass
