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
    double startTotal = TotalRateAt(brick, contentDelta, 0);
    StoreRatesAtCurrentContent(brick, _startRates);

    // Solve g(S) = S - S0 - h (I - (Q0 + Q(S)) / 2) = 0 for the end-of-step content S
    // by bisection. Q is non-decreasing in S, so g is increasing and the root is
    // bracketed by the no-outflow bound above and the max-outflow bound below.
    double h = timeStepInDays;
    double hi = content + h * std::max(inflow, 0.0);
    double maxOutflow = TotalRateAt(brick, contentDelta, hi - content);
    double lo = content + h * (inflow - (startTotal + maxOutflow) / 2);
    if (!container->AllowsNegativeContent()) {
        lo = std::max(lo, 0.0);
    }

    double endContent = hi;
    if (hi > lo) {
        for (int iter = 0; iter < 100; ++iter) {
            endContent = (lo + hi) / 2;
            double endTotal = TotalRateAt(brick, contentDelta, endContent - content);
            double g = endContent - content - h * (inflow - (startTotal + endTotal) / 2);
            if (g > 0) {
                hi = endContent;
            } else {
                lo = endContent;
            }
            if (hi - lo < 1e-12) {
                break;
            }
        }
        endContent = (lo + hi) / 2;
    } else {
        endContent = lo;
    }

    // Applied rates: trapezoidal average of the start- and end-of-step process rates.
    TotalRateAt(brick, contentDelta, endContent - content);
    StoreRatesAtCurrentContent(brick, _endRates);
    assert(_startRates.size() == _endRates.size());
    for (int i = 0; i < _startRates.size(); ++i) {
        _rates(iRateStart + i) = (_startRates[i] + _endRates[i]) / 2;
    }

    // Restore the start-of-step state for the constraint and application passes.
    *contentDelta = 0;
}
