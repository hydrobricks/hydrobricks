#ifndef HYDROBRICKS_UNIT_HYDROGRAPH_GR_H
#define HYDROBRICKS_UNIT_HYDROGRAPH_GR_H

#include <cmath>
#include <vector>

/**
 * Shared GR unit-hydrograph S-curves and ordinate computation (Perrin et al., 2003).
 *
 * Used by the GR6J routing process. GR4J keeps its own equivalent copy embedded
 * in ProcessRoutingGR4J so that class stays untouched.
 */
namespace gr_uh {

/**
 * SH1 cumulative S-curve, time base x4.
 */
inline double SH1(double t, double x4) {
    if (t <= 0.0) {
        return 0.0;
    }
    if (t >= x4) {
        return 1.0;
    }
    return std::pow(t / x4, 2.5);
}

/**
 * SH2 cumulative S-curve, time base 2*x4.
 */
inline double SH2(double t, double x4) {
    if (t <= 0.0) {
        return 0.0;
    }
    if (t >= 2.0 * x4) {
        return 1.0;
    }
    if (t < x4) {
        return 0.5 * std::pow(t / x4, 2.5);
    }
    return 1.0 - 0.5 * std::pow(2.0 - t / x4, 2.5);
}

/**
 * Fill the UH1 and UH2 ordinates (successive differences of the S-curves) for the
 * given time base x4 (clamped to a minimum of 0.5). The ordinate vectors are
 * resized to ceil(x4) and ceil(2*x4) respectively.
 *
 * @param x4 unit-hydrograph time base [d].
 * @param uh1Ord output UH1 ordinates.
 * @param uh2Ord output UH2 ordinates.
 */
inline void ComputeOrdinates(double x4, std::vector<double>& uh1Ord, std::vector<double>& uh2Ord) {
    if (x4 <= 0.0) {
        x4 = 0.5;
    }
    int n1 = static_cast<int>(std::ceil(x4));
    int n2 = static_cast<int>(std::ceil(2.0 * x4));

    uh1Ord.resize(n1);
    for (int j = 1; j <= n1; ++j) {
        uh1Ord[j - 1] = SH1(static_cast<double>(j), x4) - SH1(static_cast<double>(j - 1), x4);
    }

    uh2Ord.resize(n2);
    for (int j = 1; j <= n2; ++j) {
        uh2Ord[j - 1] = SH2(static_cast<double>(j), x4) - SH2(static_cast<double>(j - 1), x4);
    }
}

}  // namespace gr_uh

#endif  // HYDROBRICKS_UNIT_HYDROGRAPH_GR_H
