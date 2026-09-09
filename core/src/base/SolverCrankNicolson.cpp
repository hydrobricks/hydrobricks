#include "SolverCrankNicolson.h"

#include <cmath>

#include "Brick.h"
#include "Processor.h"
#include "WaterContainer.h"

SolverCrankNicolson::SolverCrankNicolson()
    : SolverSequential() {}

void SolverCrankNicolson::ComputeBrickRates(Brick* brick, double content, double inflow, double timeStepInDays,
                                            int iRateStart) {
    WaterContainer* container = brick->GetWaterContainer();
    double* contentDelta = container->GetDynamicContentChanges()[0];
    assert(*contentDelta == 0);

    // Rates at the start-of-step content.
    double startTotal = StoreRatesAndTotalAt(brick, contentDelta, 0, _startRates);

    // Solve g(S) = S - S0 - h (I - (Q0 + Q(S)) / 2) = 0 for the end-of-step content S.
    // Q is non-decreasing in S, so g is increasing and the root is bracketed by the
    // no-outflow bound above and the max-outflow bound below.
    double h = timeStepInDays;
    double hi = content + h * std::max(inflow, 0.0);
    double maxOutflow = TotalRateAt(brick, contentDelta, hi - content);
    double lo = content + h * (inflow - (startTotal + maxOutflow) / 2);
    if (!container->AllowsNegativeContent()) {
        lo = std::max(lo, 0.0);
    }

    // dg/dS = 1 + h/2 dQ/dS >= 1, so |S - root| <= |g(S)|: converging on |g| gives the
    // same accuracy guarantee as shrinking the bracket, and the Illinois iteration
    // (regula falsi with the stalled endpoint halved) reaches it in a handful of rate
    // evaluations where plain bisection needed about fifty of them per brick per step.
    constexpr double tolerance = 1e-12;
    auto evaluateG = [&](double s) {
        double total = TotalRateAt(brick, contentDelta, s - content);
        return s - content - h * (inflow - (startTotal + total) / 2);
    };

    double endContent;
    if (hi <= lo) {
        endContent = lo;
    } else {
        double gLo = evaluateG(lo);
        double gHi = hi - content - h * (inflow - (startTotal + maxOutflow) / 2);
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

    // Applied rates: trapezoidal average of the start- and end-of-step process rates.
    StoreRatesAndTotalAt(brick, contentDelta, endContent - content, _endRates);
    assert(_startRates.size() == _endRates.size());
    for (int i = 0; i < _startRates.size(); ++i) {
        _rates(iRateStart + i) = (_startRates[i] + _endRates[i]) / 2;
    }

    // Restore the start-of-step state for the constraint and application passes.
    *contentDelta = 0;
}
