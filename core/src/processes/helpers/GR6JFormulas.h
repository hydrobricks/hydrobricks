#ifndef HYDROBRICKS_GR6J_FORMULAS_H
#define HYDROBRICKS_GR6J_FORMULAS_H

#include <algorithm>
#include <cmath>

/**
 * Shared GR6J exponential-store equation (Pushpalatha et al., 2011; Michel et al., 2003).
 *
 * Follows the airGR RunModel_GR6J formulation. The store level Rexp may be
 * negative (no non-negativity constraint), which is what lets the exponential
 * store simulate long, smooth recession spells.
 */
namespace gr6j {

/**
 * Outflow from the GR6J exponential routing store.
 *
 * @param Rexp current exponential-store level [mm] (may be negative).
 * @param X6 exponential-store coefficient [mm] (> 0).
 * @return the outflow [mm].
 */
inline double ExponentialStoreOutflow(double Rexp, double X6) {
    if (X6 <= 0.0) {
        // Degenerate store: drain whatever positive content is present.
        return std::max(0.0, Rexp);
    }

    // ar is clamped to [-33, 33] to keep exp() in a safe numeric range, then the
    // outflow is computed with a piecewise form that stays accurate at both ends.
    double ar = std::max(-33.0, std::min(33.0, Rexp / X6));
    if (ar > 7.0) {
        return Rexp + X6 / std::exp(ar);
    }
    if (ar < -7.0) {
        return X6 * std::exp(ar);
    }
    return X6 * std::log(std::exp(ar) + 1.0);
}

}  // namespace gr6j

#endif  // HYDROBRICKS_GR6J_FORMULAS_H
