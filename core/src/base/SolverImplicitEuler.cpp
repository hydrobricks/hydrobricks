#include "SolverImplicitEuler.h"

#include <cmath>

#include "Brick.h"
#include "Processor.h"
#include "WaterContainer.h"

SolverImplicitEuler::SolverImplicitEuler()
    : SolverSequential() {}

void SolverImplicitEuler::ComputeBrickRates(Brick* brick, double content, double inflow, double timeStepInDays,
                                            int iRateStart) {
    WaterContainer* container = brick->GetWaterContainer();
    double* contentDelta = container->GetDynamicContentChanges()[0];
    assert(*contentDelta == 0);

    // Solve g(S) = S - S0 - h (I - Q(S)) = 0 for the end-of-step content S. Q is
    // non-decreasing in S, so g is increasing, and dg/dS = 1 + h dQ/dS >= 1, which bounds
    // the distance to the root by the residual: |S - root| <= |g(S)|. Any content whose
    // residual is within the tolerance is therefore an acceptable answer, however it was
    // obtained. Every evaluation below stores the per-connection rates in _endRates, so the
    // accepted content leaves behind the rates that are applied.
    double h = timeStepInDays;
    constexpr double tolerance = 1e-12;
    auto residualAt = [&](double s) {
        double total = StoreRatesAndTotalAt(brick, contentDelta, s - content, _endRates);
        return s - content - h * (inflow - total);
    };

    bool solved = false;

    // Affine shortcut. When every process responds affinely to the content (Q = k S - offset),
    // g is affine as well and its root is available in closed form, which spares the whole
    // bracketing search. The coefficients only describe the processes over the range where
    // they stay affine (a threshold outflow switches off below its threshold, an empty store
    // produces no outflow at all), so the closed-form content is only a candidate: it is
    // accepted once its residual, computed from the real process rates, passes the same test
    // as the iteration below, and discarded otherwise.
    double linearCoefficient = 0;
    double linearOffset = 0;
    if (SumAffineResponse(brick, linearCoefficient, linearOffset) && linearCoefficient > 0) {
        double candidate = (content + h * (inflow + linearOffset)) / (1 + h * linearCoefficient);
        if (candidate > 0 || container->AllowsNegativeContent()) {
            solved = std::abs(residualAt(candidate)) <= tolerance;
        }
    }

    if (!solved) {
        // The root is bracketed by the no-outflow bound above and the max-outflow bound below.
        double hi = content + h * std::max(inflow, 0.0);
        double maxOutflow = StoreRatesAndTotalAt(brick, contentDelta, hi - content, _endRates);
        double lo = content + h * (inflow - maxOutflow);
        if (!container->AllowsNegativeContent()) {
            lo = std::max(lo, 0.0);
        }
        double gHi = hi - content - h * (inflow - maxOutflow);

        if (hi <= lo) {
            // Empty bracket: the low bound is the answer.
            StoreRatesAndTotalAt(brick, contentDelta, lo - content, _endRates);
        } else if (gHi > 0) {
            double gLo = residualAt(lo);
            if (gLo < 0) {
                // Illinois iteration: regula falsi with the stalled endpoint halved. It keeps
                // the bracket and converges in a handful of evaluations, where plain bisection
                // needed about fifty of them per brick per step.
                int retainedSide = 0;
                for (int iter = 0; iter < 100; ++iter) {
                    double s = (lo * gHi - hi * gLo) / (gHi - gLo);
                    if (!(s > lo && s < hi)) {
                        s = (lo + hi) / 2;
                    }
                    double gS = residualAt(s);
                    if (std::abs(gS) <= tolerance || hi - lo <= tolerance) {
                        break;
                    }
                    if (gS > 0) {
                        hi = s;
                        gHi = gS;
                        if (retainedSide == 1) {
                            gLo /= 2;
                        }
                        retainedSide = 1;
                    } else {
                        lo = s;
                        gLo = gS;
                        if (retainedSide == -1) {
                            gHi /= 2;
                        }
                        retainedSide = -1;
                    }
                }
            }
            // gLo >= 0: the low bound is the root, and _endRates holds the rates it was
            // evaluated with.
        }
        // gHi <= 0: the high bound is the root, and _endRates holds the rates of the
        // max-outflow evaluation, which was made at that very content.
    }

    // The applied rates are the end-of-step process rates.
    for (int i = 0; i < _endRates.size(); ++i) {
        _rates(iRateStart + i) = _endRates[i];
    }

    // Restore the start-of-step state for the constraint and application passes.
    *contentDelta = 0;
}
