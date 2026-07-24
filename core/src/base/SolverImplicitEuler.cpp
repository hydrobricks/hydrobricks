#include "SolverImplicitEuler.h"

#include <cmath>

#include "Brick.h"
#include "Processor.h"
#include "WaterContainer.h"

SolverImplicitEuler::SolverImplicitEuler()
    : SolverSequential() {}

double SolverImplicitEuler::TotalRateAt(Brick* brick, double* contentDelta, double offset) {
    *contentDelta = offset;
    double total = 0;
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        const vecDouble& processRates = brick->GetProcess(i)->GetChangeRates();
        for (int j = 0; j < processRates.size(); ++j) {
            total += processRates[j];
        }
    }
    return total;
}

void SolverImplicitEuler::ComputeBrickRates(Brick* brick, double content, double inflow, double timeStepInDays,
                                            int iRateStart) {
    WaterContainer* container = brick->GetWaterContainer();
    double* contentDelta = container->GetDynamicContentChanges()[0];
    assert(*contentDelta == 0);

    // Solve g(S) = S - S0 - h (I - Q(S)) = 0 for the end-of-step content S by
    // bisection. Q is non-decreasing in S, so g is increasing and the root is
    // bracketed by the no-outflow bound above and the max-outflow bound below.
    double h = timeStepInDays;
    double hi = content + h * std::max(inflow, 0.0);
    double maxOutflow = TotalRateAt(brick, contentDelta, hi - content);
    double lo = content + h * (inflow - maxOutflow);
    if (!container->AllowsNegativeContent()) {
        lo = std::max(lo, 0.0);
    }

    double endContent = hi;
    if (hi > lo) {
        for (int iter = 0; iter < 100; ++iter) {
            endContent = (lo + hi) / 2;
            double g = endContent - content - h * (inflow - TotalRateAt(brick, contentDelta, endContent - content));
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

    // Store the per-process rates evaluated at the end-of-step content.
    TotalRateAt(brick, contentDelta, endContent - content);
    int iRate = iRateStart;
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        const vecDouble& processRates = brick->GetProcess(i)->GetChangeRates();
        for (int j = 0; j < processRates.size(); ++j) {
            _rates(iRate) = processRates[j];
            iRate++;
        }
    }

    // Restore the start-of-step state for the constraint and application passes.
    *contentDelta = 0;
}
