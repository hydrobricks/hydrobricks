#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <stdexcept>
#include <string>

/**
 * @file Exceptions.h
 * @brief Exception hierarchy for Hydrobricks
 *
 * Exception semantics based on WHO is responsible and WHEN detected:
 *
 * - HydrobricksError: Root exception for all hydrobricks-specific errors
 *
 * - Input validation errors (std::invalid_argument semantics) - USER's fault, detected at SETUP:
 *   - InputError: Generic bad user input (invalid values, malformed configuration)
 *   - ModelConfigError: Model configuration error (unsupported operations, incompatible components)
 *
 * - Internal invariant violations (std::logic_error semantics) - PROGRAMMER's fault:
 *   - NotImplemented: Feature not yet coded (legitimate but incomplete)
 *   - ShouldNotHappen: Code path that should never execute (internal bug)
 *
 * - Runtime errors (std::runtime_error base) - SYSTEM's fault, detected at RUNTIME:
 *   - RuntimeError: Unexpected runtime condition (I/O failures, system resource issues)
 */

// ============================================================================
// Root Exception
// ============================================================================

/**
 * @class HydrobricksError
 * @brief Root exception class for all Hydrobricks-specific errors
 *
 * All custom Hydrobricks exceptions ultimately inherit from this class,
 * allowing for uniform exception handling at the top level.
 */
class HydrobricksError : public std::runtime_error {
  public:
    explicit HydrobricksError(const char* msg)
        : std::runtime_error(msg) {}

    explicit HydrobricksError(const std::string& msg)
        : std::runtime_error(msg) {}

    virtual ~HydrobricksError() = default;
};

// ============================================================================
// Input Validation Errors (std::invalid_argument base) - User's fault
// ============================================================================

/**
 * @class InputError
 * @brief Exception for invalid user input
 *
 * Used when user provides bad input values, invalid parameter values.
 * Semantically represents std::invalid_argument (user error in setup).
 *
 * This should be used for validation errors that result from user input,
 * not from internal logic errors.
 */
class InputError : public HydrobricksError {
  public:
    explicit InputError(const char* msg)
        : HydrobricksError(msg) {}

    explicit InputError(const std::string& msg)
        : HydrobricksError(msg) {}

    virtual ~InputError() = default;
};

/**
 * @class ModelConfigError
 * @brief Exception for model configuration or conception errors
 *
 * Used when there is a fundamental mismatch in how the model is configured by the user,
 * such as:
 * - Applying unsupported processes to incompatible brick types
 * - Invalid coupling between incompatible model components
 * - Configuration that violates the conceptual model design
 * - Invalid fractions or ratios in configuration
 *
 * Semantically represents std::invalid_argument (user error in problem setup).
 * This indicates an error in the user's problem setup/conception detected during model construction.
 */
class ModelConfigError : public HydrobricksError {
  public:
    explicit ModelConfigError(const char* msg)
        : HydrobricksError(msg) {}

    explicit ModelConfigError(const std::string& msg)
        : HydrobricksError(msg) {}

    virtual ~ModelConfigError() = default;
};

// ============================================================================
// Internal Logic Errors (std::logic_error base) - Programmer's fault
// ============================================================================

/**
 * @class NotImplemented
 * @brief Exception for unimplemented features
 *
 * Used when a feature or code path is not yet implemented.
 * Semantically represents std::logic_error (programmer's incomplete implementation).
 * This should be used for legitimate features that are planned but incomplete.
 */
class NotImplemented : public HydrobricksError {
  public:
    NotImplemented()
        : HydrobricksError("Function not yet implemented") {}

    explicit NotImplemented(const char* msg)
        : HydrobricksError(msg) {}

    explicit NotImplemented(const std::string& msg)
        : HydrobricksError(msg) {}

    virtual ~NotImplemented() = default;
};

/**
 * @class ShouldNotHappen
 * @brief Exception for code paths that should never be reached
 *
 * Used when execution reaches a code path that represents an internal logic error
 * or contract violation. Semantically represents std::logic_error (programmer bug).
 * This indicates a bug in the program itself, not user error.
 */
class ShouldNotHappen : public HydrobricksError {
  public:
    ShouldNotHappen()
        : HydrobricksError("This should not happen...") {}

    explicit ShouldNotHappen(const char* msg)
        : HydrobricksError(msg) {}

    explicit ShouldNotHappen(const std::string& msg)
        : HydrobricksError(msg) {}

    virtual ~ShouldNotHappen() = default;
};

// ============================================================================
// Runtime Errors (std::runtime_error base)
// ============================================================================

/**
 * @class RuntimeError
 * @brief Exception for unexpected runtime conditions
 *
 * Used for errors that occur during execution and cannot be predicted
 * at configuration time, such as I/O failures or numerical issues.
 */
class RuntimeError : public HydrobricksError {
  public:
    explicit RuntimeError(const char* msg)
        : HydrobricksError(msg) {}

    explicit RuntimeError(const std::string& msg)
        : HydrobricksError(msg) {}

    virtual ~RuntimeError() = default;
};

#endif  // EXCEPTIONS_H
