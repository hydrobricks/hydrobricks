#ifndef HYDROBRICKS_NUMERIC_CONSTANTS_H
#define HYDROBRICKS_NUMERIC_CONSTANTS_H

#include <cmath>
#include <limits>

/**
 * @file NumericConstants.h
 * @brief Centralized numeric tolerance policy and comparison utilities.
 *
 * This file provides consistent tolerance values and helper functions for
 * floating-point comparisons throughout the codebase.
 */

namespace NumericConstants {

/**
 * Machine epsilon for double precision floating-point numbers.
 * This is the smallest value such that 1.0 + EPSILON_D != 1.0
 */
static const double EPSILON_D = std::numeric_limits<double>::epsilon();

/**
 * Machine epsilon for single precision floating-point numbers.
 * This is the smallest value such that 1.0f + EPSILON_F != 1.0f
 */
static const double EPSILON_F = std::numeric_limits<float>::epsilon();

/**
 * Standard precision tolerance for practical comparisons.
 * Used when machine epsilon is too strict for real-world calculations.
 * Value: 1e-8 (0.00000001)
 */
static const double PRECISION = 0.00000001;

/**
 * Relaxed tolerance for sum-of-fractions checks and similar operations.
 * Used when dealing with accumulated rounding errors or user-provided values.
 * Value: 1e-4 (0.0001)
 */
static const double TOLERANCE_LOOSE = 0.0001;

/**
 * Compare two floating-point values for near-equality.
 *
 * @param a First value to compare
 * @param b Second value to compare
 * @param tolerance Tolerance threshold for comparison (default: PRECISION)
 * @return true if |a - b| <= tolerance, false otherwise
 */
inline bool NearlyEqual(double a, double b, double tolerance = PRECISION) {
    return std::fabs(a - b) <= tolerance;
}

/**
 * Check if a value is nearly zero.
 *
 * @param value Value to check
 * @param tolerance Tolerance threshold for comparison (default: PRECISION)
 * @return true if |value| <= tolerance, false otherwise
 */
inline bool NearlyZero(double value, double tolerance = PRECISION) {
    return std::fabs(value) <= tolerance;
}

/**
 * Check if value a is greater than value b with tolerance.
 *
 * @param a First value
 * @param b Second value
 * @param tolerance Tolerance threshold for comparison (default: PRECISION)
 * @return true if a > b + tolerance, false otherwise
 */
inline bool GreaterThan(double a, double b, double tolerance = PRECISION) {
    return a > b + tolerance;
}

/**
 * Check if value a is less than value b with tolerance.
 *
 * @param a First value
 * @param b Second value
 * @param tolerance Tolerance threshold for comparison (default: PRECISION)
 * @return true if a < b - tolerance, false otherwise
 */
inline bool LessThan(double a, double b, double tolerance = PRECISION) {
    return a < b - tolerance;
}

/**
 * Check if value a is greater than or equal to value b with tolerance.
 *
 * @param a First value
 * @param b Second value
 * @param tolerance Tolerance threshold for comparison (default: PRECISION)
 * @return true if a >= b - tolerance, false otherwise
 */
inline bool GreaterThanOrEqual(double a, double b, double tolerance = PRECISION) {
    return a >= b - tolerance;
}

/**
 * Check if value a is less than or equal to value b with tolerance.
 *
 * @param a First value
 * @param b Second value
 * @param tolerance Tolerance threshold for comparison (default: PRECISION)
 * @return true if a <= b + tolerance, false otherwise
 */
inline bool LessThanOrEqual(double a, double b, double tolerance = PRECISION) {
    return a <= b + tolerance;
}

}  // namespace NumericConstants

// Expose constants in global namespace
using NumericConstants::EPSILON_D;
using NumericConstants::EPSILON_F;
using NumericConstants::PRECISION;
using NumericConstants::TOLERANCE_LOOSE;

// Convenience functions in global namespace
using NumericConstants::NearlyEqual;
using NumericConstants::NearlyZero;
using NumericConstants::GreaterThan;
using NumericConstants::LessThan;
using NumericConstants::GreaterThanOrEqual;
using NumericConstants::LessThanOrEqual;

#endif  // HYDROBRICKS_NUMERIC_CONSTANTS_H

