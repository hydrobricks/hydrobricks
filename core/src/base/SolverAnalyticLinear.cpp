#include "SolverAnalyticLinear.h"

#include <cmath>

#include "Brick.h"
#include "Processor.h"
#include "WaterContainer.h"

SolverAnalyticLinear::SolverAnalyticLinear()
    : SolverSequential() {}

void SolverAnalyticLinear::ComputeBrickRates(Brick* brick, double content, double inflow, double timeStepInDays,
                                             int iRateStart) {
    // Partition the processes: linear responses (rate = k * S) are integrated
    // exactly; the other rates are frozen at their start-of-step value.
    int iRate = iRateStart;
    double totalLinearCoefficient = 0;  // sum of the linear coefficients k [1/d]
    double totalFrozenRate = 0;         // sum of the frozen rates [mm/d]
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        auto process = brick->GetProcess(i);

        if (process->HasLinearResponse() && process->GetConnectionCount() == 1) {
            totalLinearCoefficient += process->GetLinearResponseRate();
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

    // Exact integration of dS/dt = I_net - k S over the step (I_net constant).
    double netInflow = inflow - totalFrozenRate;
    double decay = std::exp(-totalLinearCoefficient * timeStepInDays);
    double newContent = content * decay + netInflow / totalLinearCoefficient * (1.0 - decay);
    // Total linear outflow volume by mass balance, as an average rate over the step.
    double linearOutflowRate = (netInflow * timeStepInDays - (newContent - content)) / timeStepInDays;
    linearOutflowRate = std::max(linearOutflowRate, 0.0);

    // Distribute over the linear processes proportionally to their coefficients.
    int iRateFill = iRateStart;
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        auto process = brick->GetProcess(i);
        if (process->HasLinearResponse() && process->GetConnectionCount() == 1) {
            _rates(iRateFill) = linearOutflowRate * process->GetLinearResponseRate() / totalLinearCoefficient;
            iRateFill++;
        } else {
            iRateFill += process->GetConnectionCount();
        }
    }
}
