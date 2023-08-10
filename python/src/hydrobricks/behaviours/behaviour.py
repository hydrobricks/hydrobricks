from abc import ABC, abstractmethod


class Behaviour(ABC):
    """Base class for the behaviours"""

    @abstractmethod
    def __init__(self):
        pass
