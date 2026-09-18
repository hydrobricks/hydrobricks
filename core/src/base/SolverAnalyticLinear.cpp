#include "SolverAnalyticLinear.h"

#include <cmath>

#include "Brick.h"
#include "Processor.h"
#include "WaterContainer.h"

SolverAnalyticLinear::SolverAnalyticLinear()
    : SolverSequential() {}

void SolverAnalyticLinear::ComputeBrickRates(Brick* brick, double content, double inflow, double timeStepInDays,
                                             int iRateStart) {
    // Partition the processes: affine responses (rate = k * S - offset) are integrated
    // exactly; the other rates are frozen at their start-of-step value.
    int iRate = iRateStart;
    double totalLinearCoefficient = 0;  // sum of the linear coefficients k [1/d]
    double totalLinearOffset = 0;       // sum of the offsets k * threshold [mm/d]
    double totalFrozenRate = 0;         // sum of the frozen rates [mm/d]
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        auto process = brick->GetProcess(i);

        if (process->HasLinearResponse() && process->GetConnectionCount() == 1) {
            totalLinearCoefficient += process->GetLinearResponseRate();
            totalLinearOffset += process->GetLinearResponseOffset();
            _rates(iRate) = 0;  // Filled below once the total linear outflow is known.
            iRate++;
        } else {
            const vecDouble& processRates = process->GetChangeRates();
            for (int j = 0; j < processRates.size(); ++j) {
                _rates(iRate) = processRates[j];
                totalFrozenRate += processRates[j];
                iRate++;
            }
        }
    }

    if (totalLinearCoefficient <= 0) {
        return;
    }

    // Exact integration of dS/dt = (I_net + offset) - k S over the step (I_net constant).
    double netInflow = inflow - totalFrozenRate;
    double decay = std::exp(-totalLinearCoefficient * timeStepInDays);
    double drivingRate = netInflow + totalLinearOffset;
    double newContent = content * decay + drivingRate / totalLinearCoefficient * (1.0 - decay);
    // Total outflow volume of the affine processes, by mass balance over the step.
    double linearOutflowVolume = netInflow * timeStepInDays - (newContent - content);
    linearOutflowVolume = std::max(linearOutflowVolume, 0.0);
    // Time integral of the content, from that volume: V = k * int(S) - offset * dt.
    double contentIntegral = (linearOutflowVolume + totalLinearOffset * timeStepInDays) / totalLinearCoefficient;

    // Distribute over the affine processes: each drains k_i * int(S) - offset_i * dt.
    int iRateFill = iRateStart;
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        auto process = brick->GetProcess(i);
        if (process->HasLinearResponse() && process->GetConnectionCount() == 1) {
            double volume = process->GetLinearResponseRate() * contentIntegral -
                            process->GetLinearResponseOffset() * timeStepInDays;
            // A store starting above its threshold may fall below it within the step; the
            // affine solution then over-drains, so the outflow is clamped (the water simply
            // stays in the store, the mass balance being carried by the applied rates).
            _rates(iRateFill) = std::max(volume, 0.0) / timeStepInDays;
            iRateFill++;
        } else {
            iRateFill += process->GetConnectionCount();
        }
    }
}
