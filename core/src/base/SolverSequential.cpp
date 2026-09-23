#include "SolverSequential.h"

#include "Brick.h"
#include "Processor.h"
#include "WaterContainer.h"

void SolverSequential::InitializeContainers() {
    assert(_processor);
    _rates = axd::Zero(_processor->GetSolvableConnectionCount());

    // Note on capacity-limited stores without an overflow process: the sequential scheme
    // cannot retroactively reduce inflows that upstream bricks have already applied, so a
    // capacity that actually binds against solver-brick inflow cannot route its excess and
    // would break the mass balance. This is left to the runtime constraint enforcement
    // (WaterContainer::ApplyConstraints), which routes excess through an overflow when one
    // exists and throws for forcing directly overfilling a capped store. A capacity used
    // only as a process parameter with a self-limiting inflow (e.g. the GR4J production
    // store, whose infiltration tends to zero as the store fills) never binds and needs no
    // overflow, so it must not be rejected here.
}

double SolverSequential::TotalRateAt(Brick* brick, double* contentDelta, double offset) {
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

double SolverSequential::StoreRatesAndTotalAt(Brick* brick, double* contentDelta, double offset, vecDouble& rates) {
    *contentDelta = offset;
    rates.clear();
    double total = 0;
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        const vecDouble& processRates = brick->GetProcess(i)->GetChangeRates();
        for (double rate : processRates) {
            total += rate;
        }
        rates.insert(rates.end(), processRates.begin(), processRates.end());
    }
    return total;
}

bool SolverSequential::SumAffineResponse(Brick* brick, double& rate, double& offset) {
    rate = 0;
    offset = 0;
    int processCount = static_cast<int>(brick->GetProcessCount());
    if (processCount == 0) {
        return false;
    }

    for (int i = 0; i < processCount; ++i) {
        Process* process = brick->GetProcess(i);
        if (!process->HasLinearResponse() || process->GetConnectionCount() != 1) {
            return false;
        }
        rate += process->GetLinearResponseRate();
        offset += process->GetLinearResponseOffset();
    }

    return true;
}

bool SolverSequential::Solve(double timeStepInDays) {
    // Sequential forward substitution: each brick is solved with its upstream inflows of
    // the current step already booked in its incoming flux amounts.
    const vector<Processor::SolvableProcess>& processes = _processor->GetSolvableProcesses();
    for (const auto& brickEntry : _processor->GetSolvableBrickEntries()) {
        // Link the brick's connections to its slice of the rates vector.
        for (int i = brickEntry.processStart; i < brickEntry.processEnd; ++i) {
            const Processor::SolvableProcess& entry = processes[i];
            for (int j = 0; j < entry.connectionCount; ++j) {
                assert(_rates.size() > entry.rateOffset + j);
                _rates(entry.rateOffset + j) = 0;
                entry.process->StoreInOutgoingFlux(&_rates(entry.rateOffset + j), j);
            }
        }

        Brick* brick = brickEntry.brick;
        if (brick->IsNull() || brickEntry.processStart == brickEntry.processEnd) {
            continue;
        }

        WaterContainer* container = brick->GetWaterContainer();
        // Start-of-step content, including instantaneous deposits from the direct pass.
        double content = container->GetContentWithChanges();
        // Inflows (upstream outflows, forcing, static handoffs) as a constant rate [mm/d].
        double inflow = container->SumIncomingFluxes() / timeStepInDays;

        ComputeBrickRates(brick, content, inflow, timeStepInDays, processes[brickEntry.processStart].rateOffset);

        // Standard constraint enforcement on the average rates (non-negative content,
        // capacity via the overflow process), then application.
        brick->ApplyConstraints(timeStepInDays);
        brick->UpdateContentFromInputs(timeStepInDays);
        for (int i = brickEntry.processStart; i < brickEntry.processEnd; ++i) {
            const Processor::SolvableProcess& entry = processes[i];
            for (int j = 0; j < entry.connectionCount; ++j) {
                entry.process->ApplyChange(j, _rates(entry.rateOffset + j), timeStepInDays);
            }
        }
    }

    _processor->FinalizeTimeStep();

    return true;
}
