#ifndef HYDROBRICKS_GR4J_FORMULAS_H
#define HYDROBRICKS_GR4J_FORMULAS_H

#include <algorithm>
#include <cmath>

/**
 * Shared GR4J production-store equations (Perrin et al., 2003).
 *
 * These are deterministic functions of the production store level S at the
 * start of the time step, the net forcing (Pn or En) and the capacity X1.
 * They are used by the discrete production process and the ET process so that
 * both compute identical, solver-independent values within a time step.
 */
namespace gr4j {

// 13 is the value at which tanh(WS) is numerically indistinguishable from 1.
constexpr double kTanhCap = 13.0;
// (9/4)^4 = 25.62890625, the percolation denominator constant.
constexpr double kPercDenom = 25.62890625;

/**
 * Infiltration into the production store: the fraction of net precipitation Pn
 * that fills the store.
 */
inline double Infiltration(double S, double Pn, double X1) {
    if (Pn <= 0.0 || X1 <= 0.0) {
        return 0.0;
    }
    double Sr = S / X1;
    double tws = std::tanh(std::min(Pn / X1, kTanhCap));
    return X1 * (1.0 - Sr * Sr) * tws / (1.0 + Sr * tws);
}

/**
 * Actual evapotranspiration drawn from the production store given the net
 * evaporative demand En.
 */
inline double Evaporation(double S, double En, double X1) {
    if (En <= 0.0 || X1 <= 0.0) {
        return 0.0;
    }
    double Sr = S / X1;
    double tws = std::tanh(std::min(En / X1, kTanhCap));
    return S * (2.0 - Sr) * tws / (1.0 + (1.0 - Sr) * tws);
}

/**
 * Percolation leaving the production store.
 */
inline double Percolation(double S, double X1) {
    if (X1 <= 0.0) {
        return 0.0;
    }
    double ratio = S / X1;
    return S * (1.0 - std::pow(1.0 + (ratio * ratio * ratio * ratio) / kPercDenom, -0.25));
}

}  // namespace gr4j

#endif  // HYDROBRICKS_GR4J_FORMULAS_H
