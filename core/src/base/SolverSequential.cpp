#include "SolverSequential.h"

#include "Brick.h"
#include "Processor.h"
#include "WaterContainer.h"

void SolverSequential::InitializeContainers() {
    assert(_processor);
    _rates = axd::Zero(_processor->GetSolvableConnectionCount());

    // The sequential scheme cannot retroactively reduce inflows that upstream bricks have
    // already applied, so capacity excesses must be booked through an overflow process.
    for (auto brick : _processor->GetSolvableBricks()) {
        WaterContainer* container = brick->GetWaterContainer();
        if (container->HasMaximumCapacity() && !container->HasOverflow()) {
            throw ModelConfigError(
                std::format("The sequential solvers require an overflow process on bricks with a maximum "
                            "capacity, but the brick '{}' has none.",
                            brick->GetName()));
        }
    }
}

bool SolverSequential::Solve(double timeStepInDays) {
    // Sequential forward substitution: each brick is solved with its upstream inflows of
    // the current step already booked in its incoming flux amounts.
    int iRate = 0;
    for (auto brick : _processor->GetSolvableBricks()) {
        // Link the brick's connections to its slice of the rates vector.
        int iRateStart = iRate;
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);
            for (int j = 0; j < process->GetConnectionCount(); ++j) {
                assert(_rates.size() > iRate);
                _rates(iRate) = 0;
                process->StoreInOutgoingFlux(&_rates(iRate), j);
                iRate++;
            }
        }

        if (brick->IsNull()) {
            continue;
        }

        WaterContainer* container = brick->GetWaterContainer();
        // Start-of-step content, including instantaneous deposits from the direct pass.
        double content = container->GetContentWithChanges();
        // Inflows (upstream outflows, forcing, static handoffs) as a constant rate [mm/d].
        double inflow = container->SumIncomingFluxes() / timeStepInDays;

        ComputeBrickRates(brick, content, inflow, timeStepInDays, iRateStart);

        // Standard constraint enforcement on the average rates (non-negative content,
        // capacity via the overflow process), then application.
        brick->ApplyConstraints(timeStepInDays);
        brick->UpdateContentFromInputs();
        int iRateApply = iRateStart;
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
