#include "SolverExponentialEuler.h"

#include <algorithm>
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

    // Rates at the start-of-step content. An empty store reports no outflow at all, but
    // one receiving water has its rates on as soon as the water arrives: a process that
    // switches on with the content (a constant percolation) would otherwise be read as
    // off for the whole step. The rates are then taken just above the empty store.
    double h = timeStepInDays;
    double startOffset = 0;
    if (content <= PRECISION && inflow > 0 && !container->AllowsNegativeContent()) {
        startOffset = 1e-6;
    }
    double startTotal = StoreRatesAndTotalAt(brick, contentDelta, startOffset, _startRates);

    // Linearize the total outflow around the start-of-step content by a secant over the
    // range of content the step can reach: Q(S) ~ Q(S0) + k (S - S0). The secant is
    // exact for affine processes, and a rate that changes abruptly within the range
    // (a threshold) is averaged over it rather than read as a near-infinite slope.
    double scale = std::max(content, content + h * inflow);
    double delta = std::max({0.001, 0.001 * scale, h * std::max(inflow, 0.0), h * startTotal});
    double slope = (TotalRateAt(brick, contentDelta, startOffset + delta) - startTotal) / delta;

    if (slope <= 1e-10) {
        // No content dependence detected: fall back to the frozen start-of-step rates.
        for (int i = 0; i < _startRates.size(); ++i) {
            _rates(iRateStart + i) = _startRates[i];
        }
        *contentDelta = 0;
        return;
    }

    // Exact integration of the linearized equation dS/dt = I_net - k (S - S0). The secant
    // is then retaken between the start and the end content it leads to, until that end
    // content settles, so that the linear model agrees with the real rates at both ends of
    // the step. An affine store settles at once; a strongly non-linear one (a runoff
    // growing with the square of the content, filling from empty under a large inflow)
    // would otherwise end the step far from the content its outflow actually reaches.
    double netInflow = inflow - startTotal;
    auto endContentFor = [&](double k) {
        double end = content + netInflow / k * (1.0 - std::exp(-k * h));
        return container->AllowsNegativeContent() ? end : std::max(end, 0.0);
    };
    double endContent = endContentFor(slope);
    for (int iter = 0; iter < 20; ++iter) {
        double change = endContent - content;
        if (std::abs(change) <= 1e-9) {
            break;
        }
        double secant = (TotalRateAt(brick, contentDelta, startOffset + change) - startTotal) / change;
        if (secant <= 1e-10 || std::abs(secant - slope) <= 1e-9 * slope) {
            break;
        }
        // Halfway between the previous slope and the new secant, which keeps the
        // iteration from oscillating around a strongly curved outflow.
        slope = (slope + secant) / 2;
        endContent = endContentFor(slope);
    }

    // Total average outflow rate over the step by mass balance.
    double totalRate = std::max(inflow - (endContent - content) / h, 0.0);

    // Distribute over the connections proportionally to their trapezoidal-average rates,
    // each capped at the largest rate it reaches over the step (at one of its ends, the
    // content being monotonic within the step). Without the cap, a process at its full
    // rate from the start (a constant percolation) would take the share of one that only
    // grows with the content (a non-linear runoff), beyond what it can deliver. The excess
    // goes to the connections still below their cap; what none can take stays in the store.
    StoreRatesAndTotalAt(brick, contentDelta, endContent - content, _endRates);
    assert(_startRates.size() == _endRates.size());
    int count = static_cast<int>(_startRates.size());
    _capped.assign(count, false);
    for (int i = 0; i < count; ++i) {
        _rates(iRateStart + i) = 0;
    }
    double remaining = totalRate;
    while (remaining > 0) {
        double sumWeights = 0;
        for (int i = 0; i < count; ++i) {
            if (!_capped[i]) {
                sumWeights += _startRates[i] + _endRates[i];
            }
        }
        if (sumWeights <= 0) {
            break;
        }
        bool newlyCapped = false;
        double passRemaining = remaining;
        for (int i = 0; i < count; ++i) {
            double cap = std::max(_startRates[i], _endRates[i]);
            if (!_capped[i] && passRemaining * (_startRates[i] + _endRates[i]) / sumWeights > cap) {
                _rates(iRateStart + i) = cap;
                _capped[i] = true;
                remaining -= cap;
                newlyCapped = true;
            }
        }
        if (!newlyCapped) {
            for (int i = 0; i < count; ++i) {
                if (!_capped[i]) {
                    _rates(iRateStart + i) = remaining * (_startRates[i] + _endRates[i]) / sumWeights;
                }
            }
            break;
        }
    }

    // Restore the start-of-step state for the constraint and application passes.
    *contentDelta = 0;
}
