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
    // non-decreasing in S, so g is increasing and the root is bracketed by the
    // no-outflow bound above and the max-outflow bound below.
    double h = timeStepInDays;
    double hi = content + h * std::max(inflow, 0.0);
    double maxOutflow = TotalRateAt(brick, contentDelta, hi - content);
    double lo = content + h * (inflow - maxOutflow);
    if (!container->AllowsNegativeContent()) {
        lo = std::max(lo, 0.0);
    }

    // dg/dS = 1 + h dQ/dS >= 1, so |S - root| <= |g(S)|: converging on |g| gives the same
    // accuracy guarantee as shrinking the bracket, and the Illinois iteration (regula
    // falsi with the stalled endpoint halved) reaches it in a handful of rate evaluations
    // where plain bisection needed about fifty of them per brick per step.
    constexpr double tolerance = 1e-12;
    auto evaluateG = [&](double s) {
        return s - content - h * (inflow - TotalRateAt(brick, contentDelta, s - content));
    };

    double endContent;
    if (hi <= lo) {
        endContent = lo;
    } else {
        double gLo = evaluateG(lo);
        double gHi = hi - content - h * (inflow - maxOutflow);
        if (gLo >= 0) {
            endContent = lo;
        } else if (gHi <= 0) {
            endContent = hi;
        } else {
            endContent = (lo + hi) / 2;
            int retainedSide = 0;
            for (int iter = 0; iter < 100; ++iter) {
                double s = (lo * gHi - hi * gLo) / (gHi - gLo);
                if (!(s > lo && s < hi)) {
                    s = (lo + hi) / 2;
                }
                double gS = evaluateG(s);
                endContent = s;
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
    }

    // Store the per-process rates evaluated at the end-of-step content.
    StoreRatesAndTotalAt(brick, contentDelta, endContent - content, _endRates);
    for (int i = 0; i < _endRates.size(); ++i) {
        _rates(iRateStart + i) = _endRates[i];
    }

    // Restore the start-of-step state for the constraint and application passes.
    *contentDelta = 0;
}
