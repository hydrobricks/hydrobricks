"""
Custom exception classes for hydrobricks.

This module defines domain-specific exceptions that provide clearer error handling
and better debugging information for hydrological modeling operations.

Only the core exception categories are defined here. These provide sufficient
granularity for error handling without overwhelming users with too many choices.
"""

from __future__ import annotations

from typing import Any


class HydroBricksError(Exception):
    """
    Base exception for all hydrobricks-specific errors.

    All custom exceptions in hydrobricks inherit from this class, allowing users
    to catch all hydrobricks-specific errors with a single except clause.

    Examples
    --------
    >>> import hydrobricks as hb
    >>> try:
    ...     model.run(parameters, forcing)
    ... except hb.HydroBricksError as e:
    ...     # Catches any hydrobricks-specific error
    ...     print(f"HydroBricks error: {e}")
    """

    pass


class DataError(HydroBricksError):
    """
    Raised for data-related errors (invalid, missing, or inconsistent data).

    Use this for issues with:
    - Invalid or malformed input data
    - Missing required data fields
    - Inconsistent data dimensions or units
    - Time series validation failures

    Attributes
    ----------
    data_type : str | None
        Type of data that caused the error (e.g., 'observations', 'DEM').
    reason : str | None
        Explanation of why the data is invalid.

    Examples
    --------
    >>> raise DataError(
    ...     f"dx must have 'x' and 'y' dimensions. Got dimensions: {dx.dims}",
    ...     data_type='DEM',
    ...     reason='Missing x or y dimension'
    ... )
    """

    def __init__(
        self, message: str, data_type: str | None = None, reason: str | None = None
    ):
        super().__init__(message, data_type, reason)
        self.data_type = data_type
        self.reason = reason


class ConfigurationError(HydroBricksError):
    """
    Raised for model configuration errors.

    Use this for issues with:
    - Parameter definitions and values
    - Parameter range violations
    - Parameter constraints not satisfied
    - Model structure definition

    Attributes
    ----------
    item_name : str | None
        Name of the item/parameter that caused the error.
    item_value : Any
        Value of the item/parameter (if applicable).
    reason : str | None
        Explanation of why the configuration is invalid.

    Examples
    --------
    >>> raise ConfigurationError(
    ...     "Parameter 'k' = 150 is outside valid range [0, 100]",
    ...     item_name='k',
    ...     item_value=150,
    ...     reason='Out of range'
    ... )
    """

    def __init__(
        self,
        message: str,
        item_name: str | None = None,
        item_value: Any = None,
        reason: str | None = None,
    ):
        super().__init__(message, item_name, item_value, reason)
        self.item_name = item_name
        self.item_value = item_value
        self.reason = reason


class ModelError(HydroBricksError):
    """
    Raised for model execution errors.

    Use this for issues with:
    - Model not initialized before running
    - Model runtime failures
    - Convergence failures in iterative solvers
    - Numerical instabilities

    Attributes
    ----------
    is_initialized : bool | None
        Whether the model was initialized.

    Examples
    --------
    >>> raise ModelError(
    ...     "Model has not been initialized. Call setup() first.",
    ...     is_initialized=False
    ... )
    """

    def __init__(self, message: str, is_initialized: bool | None = None):
        super().__init__(message, is_initialized)
        self.is_initialized = is_initialized


class ForcingError(HydroBricksError):
    """
    Raised for forcing data processing errors.

    Use this for issues with:
    - Spatialization of forcing data
    - Elevation gradient calculations
    - Regridding operations
    - PET computation

    Attributes
    ----------
    variable : str | None
        Variable that failed to process (e.g., 'temperature', 'precipitation').
    method : str | None
        Processing method that failed.

    Examples
    --------
    >>> raise ForcingError(
    ...     "Failed to spatialize temperature data using elevation gradients",
    ...     variable='temperature',
    ...     method='additive_elevation_gradient'
    ... )
    """

    def __init__(
        self, message: str, variable: str | None = None, method: str | None = None
    ):
        super().__init__(message, variable, method)
        self.variable = variable
        self.method = method


class DependencyError(HydroBricksError):
    """
    Raised when required optional dependencies are not available.

    Use this when an operation requires a package that is not installed.

    Attributes
    ----------
    package_name : str | None
        Name of the missing package.
    operation : str | None
        Operation that requires the package.
    install_command : str | None
        Suggested installation command.

    Examples
    --------
    >>> raise DependencyError(
    ...     "xarray is required for netCDF operations",
    ...     package_name='xarray',
    ...     operation='load_from_netcdf',
    ...     install_command='pip install xarray'
    ... )
    """

    def __init__(
        self,
        message: str,
        package_name: str | None = None,
        operation: str | None = None,
        install_command: str | None = None,
    ):
        super().__init__(message, package_name, operation, install_command)
        self.package_name = package_name
        self.operation = operation
        self.install_command = install_command
