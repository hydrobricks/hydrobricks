from __future__ import annotations

from abc import ABC

from hydrobricks._exceptions import ConfigurationError


class Module(ABC):
    """Base class for the pluggable sub-model modules.

    A *module* is a swappable strategy a model plugs in to vary one part of its
    structure without changing the model itself (e.g. the glacier formulation). Each
    module *category* is a direct subclass of ``Module`` (e.g. ``GlacierModule``) that
    defines the category's interface and its own registry of named concrete
    formulations. Concrete modules register under a name with the ``register``
    decorator, and a model resolves one with
    ``<Category>.get_module(name_or_instance)``.

    This base provides only the shared pluggability machinery (registry, registration
    and resolution); the category subclasses provide the actual interface.
    """

    # Registry of {name: concrete class}. Declared per category subclass so names do
    # not collide across categories. ``_category`` is the human-readable label used in
    # error messages.
    _registry: dict[str, type[Module]]
    _category: str = "module"

    @classmethod
    def register(cls, name: str):
        """Return a class decorator registering a concrete module under ``name``.

        Example
        -------
        >>> @GlacierModule.register("gsm")
        ... class GSM(GlacierModule):
        ...     ...
        """

        def decorator(module_cls: type[Module]) -> type[Module]:
            cls._registry[name] = module_cls
            return module_cls

        return decorator

    @classmethod
    def get_module(cls, module: str | Module) -> Module:
        """Resolve a module of this category from a registry name or an instance.

        Parameters
        ----------
        module
            Either a registered module name (e.g. ``'gsm'``) or a module instance of
            this category (for custom user-defined formulations).

        Returns
        -------
        Module
            The resolved module instance.

        Raises
        ------
        ConfigurationError
            If the name is not registered in this category (or an instance of another
            category is given).
        """
        if isinstance(module, cls):
            return module
        if isinstance(module, str) and module in cls._registry:
            return cls._registry[module]()

        available = ", ".join(sorted(cls._registry))
        raise ConfigurationError(
            f"The {cls._category} '{module}' is not recognised. "
            f"Available {cls._category}s: {available}.",
            item_name=cls._category,
            item_value=module if isinstance(module, str) else type(module).__name__,
            reason="Unknown module",
        )
