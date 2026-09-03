#include "SolverExponentialEuler.h"

#include <cmath>

#include "Brick.h"
#include "Processor.h"
#include "WaterContainer.h"

SolverExponentialEuler::SolverExponentialEuler()
    : SolverSequential() {}

void SolverExponentialEuler::ComputeBrickRates(Brick* brick, double content, double inflow, double timeStepInDays,
                                               int iRateStart) {
    WaterContainer* container = brick->GetWaterContainer();
    double* contentDelta = container->GetDynamicContentChanges()[0];
    assert(*contentDelta == 0);

    // Rates at the start-of-step content.
    double startTotal = TotalRateAt(brick, contentDelta, 0);
    StoreRatesAtCurrentContent(brick, _startRates);

    // Linearize the total outflow around the start-of-step content by forward finite
    // difference: Q(S) ~ Q(S0) + k (S - S0).
    double h = timeStepInDays;
    double scale = std::max(content, content + h * inflow);
    double eps = std::max(0.001, 0.001 * scale);
    double slope = (TotalRateAt(brick, contentDelta, eps) - startTotal) / eps;

    if (slope <= 1e-10) {
        // No content dependence detected: fall back to the frozen start-of-step rates.
        for (int i = 0; i < _startRates.size(); ++i) {
            _rates(iRateStart + i) = _startRates[i];
        }
        *contentDelta = 0;
        return;
    }

    // Exact integration of the linearized equation dS/dt = I_net - k (S - S0).
    double netInflow = inflow - startTotal;
    double endContent = content + netInflow / slope * (1.0 - std::exp(-slope * h));
    if (!container->AllowsNegativeContent()) {
        endContent = std::max(endContent, 0.0);
    }

    // Total average outflow rate over the step by mass balance.
    double totalRate = std::max(inflow - (endContent - content) / h, 0.0);

    // Distribute over the connections proportionally to their trapezoidal-average rates.
    TotalRateAt(brick, contentDelta, endContent - content);
    StoreRatesAtCurrentContent(brick, _endRates);
    assert(_startRates.size() == _endRates.size());
    double sumWeights = 0;
    for (int i = 0; i < _startRates.size(); ++i) {
        sumWeights += _startRates[i] + _endRates[i];
    }
    for (int i = 0; i < _startRates.size(); ++i) {
        double weight = sumWeights > 0 ? (_startRates[i] + _endRates[i]) / sumWeights : 0.0;
        _rates(iRateStart + i) = totalRate * weight;
    }

    // Restore the start-of-step state for the constraint and application passes.
    *contentDelta = 0;
}
