#include "SolverEulerExplicit.h"

#include "Processor.h"

SolverEulerExplicit::SolverEulerExplicit()
    : Solver() {}

void SolverEulerExplicit::InitializeContainers() {
    assert(_processor);
    _k1 = axd::Zero(_processor->GetSolvableConnectionCount());
}

bool SolverEulerExplicit::Solve(double timeStepInDays) {
    // k1 = f(tn, Sn), with constraints
    _processor->EvaluateRates(_k1, timeStepInDays);

    // Advance the state over the full step and commit
    _processor->ApplyRates(_k1, timeStepInDays);
    _processor->FinalizeTimeStep();

    return true;
}
