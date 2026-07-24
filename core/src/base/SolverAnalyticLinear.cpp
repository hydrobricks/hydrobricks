#include "SolverAnalyticLinear.h"

#include <cmath>

#include "Brick.h"
#include "Processor.h"
#include "WaterContainer.h"

SolverAnalyticLinear::SolverAnalyticLinear()
    : Solver() {}

void SolverAnalyticLinear::InitializeContainers() {
    assert(_processor);
    _rates = axd::Zero(_processor->GetSolvableConnectionCount());

    // The sequential scheme cannot retroactively reduce inflows that upstream bricks have
    // already applied, so capacity excesses must be booked through an overflow process.
    for (auto brick : _processor->GetSolvableBricks()) {
        WaterContainer* container = brick->GetWaterContainer();
        if (container->HasMaximumCapacity() && !container->HasOverflow()) {
            throw ModelConfigError(
                std::format("The analytic solver requires an overflow process on bricks with a maximum "
                            "capacity, but the brick '{}' has none.",
                            brick->GetName()));
        }
    }
}

bool SolverAnalyticLinear::Solve(double timeStepInDays) {
    // Sequential forward substitution: each brick is solved with its upstream inflows of
    // the current step already booked in its incoming flux amounts.
    int iRate = 0;
    for (auto brick : _processor->GetSolvableBricks()) {
        if (brick->IsNull()) {
            for (int i = 0; i < brick->GetProcessCount(); ++i) {
                auto process = brick->GetProcess(i);
                for (int j = 0; j < process->GetConnectionCount(); ++j) {
                    assert(_rates.size() > iRate);
                    _rates(iRate) = 0;
                    process->StoreInOutgoingFlux(&_rates(iRate), j);
                    iRate++;
                }
            }
            continue;
        }

        WaterContainer* container = brick->GetWaterContainer();
        // Start-of-step content, including instantaneous deposits from the direct pass.
        double content = container->GetContentWithChanges();
        // Inflows (upstream outflows, forcing, static handoffs) as a constant rate [mm/d].
        double inflow = container->SumIncomingFluxes() / timeStepInDays;

        // Partition the processes: linear responses (rate = k * S) are integrated
        // exactly; the other rates are frozen at their start-of-step value.
        int iRateBrickStart = iRate;
        double totalLinearCoefficient = 0;  // sum of the linear coefficients k [1/d]
        double totalFrozenRate = 0;         // sum of the frozen rates [mm/d]
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);

            if (process->HasLinearResponse() && process->GetConnectionCount() == 1) {
                totalLinearCoefficient += process->GetLinearResponseRate();
                assert(_rates.size() > iRate);
                _rates(iRate) = 0;  // Filled below once the total linear outflow is known.
                process->StoreInOutgoingFlux(&_rates(iRate), 0);
                iRate++;
            } else {
                const vecDouble& processRates = process->GetChangeRates();
                for (int j = 0; j < processRates.size(); ++j) {
                    assert(_rates.size() > iRate);
                    _rates(iRate) = processRates[j];
                    totalFrozenRate += processRates[j];
                    process->StoreInOutgoingFlux(&_rates(iRate), j);
                    iRate++;
                }
            }
        }

        // Exact integration of dS/dt = I_net - k S over the step (I_net constant).
        if (totalLinearCoefficient > 0) {
            double netInflow = inflow - totalFrozenRate;
            double decay = std::exp(-totalLinearCoefficient * timeStepInDays);
            double newContent = content * decay + netInflow / totalLinearCoefficient * (1.0 - decay);
            // Total linear outflow volume by mass balance, as an average rate over the step.
            double linearOutflowRate = (netInflow * timeStepInDays - (newContent - content)) / timeStepInDays;
            linearOutflowRate = std::max(linearOutflowRate, 0.0);

            // Distribute over the linear processes proportionally to their coefficients.
            int iRateFill = iRateBrickStart;
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

        // Standard constraint enforcement on the average rates (non-negative content,
        // capacity via the overflow process), then application.
        brick->ApplyConstraints(timeStepInDays);
        brick->UpdateContentFromInputs();
        int iRateApply = iRateBrickStart;
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);
            for (int j = 0; j < process->GetConnectionCount(); ++j) {
                process->ApplyChange(j, _rates(iRateApply), timeStepInDays);
                iRateApply++;
            }
        }
    }

    _processor->FinalizeTimeStep();

    return true;
}
