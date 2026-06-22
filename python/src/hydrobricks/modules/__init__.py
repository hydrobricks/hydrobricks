"""Pluggable sub-model modules for the hydrological models.

Modules are strategy components that a model can plug in to vary a part of its
structure without changing the model itself (e.g. the glacier formulation). They are
selected by a model option (a registry name) or supplied as an instance.
"""

from hydrobricks.modules.glacier import GSM, GlacierModule, get_glacier_module

__all__ = ("GlacierModule", "GSM", "get_glacier_module")
