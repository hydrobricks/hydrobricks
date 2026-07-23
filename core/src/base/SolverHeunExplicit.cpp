#include "SolverHeunExplicit.h"

#include "Processor.h"

SolverHeunExplicit::SolverHeunExplicit()
    : Solver() {}

void SolverHeunExplicit::InitializeContainers() {
    assert(_processor);
    int rateCount = _processor->GetSolvableConnectionCount();
    _k1 = axd::Zero(rateCount);
    _k2 = axd::Zero(rateCount);
    _combinedRates = axd::Zero(rateCount);
}

bool SolverHeunExplicit::Solve(double timeStepInDays) {
    // k1 = f(tn, Sn), with constraints
    _processor->EvaluateRates(_k1, timeStepInDays);

    // Advance the state to Sn + k1 h
    _processor->ApplyRates(_k1, timeStepInDays);

    // k2 = f(tn + h, Sn + k1 h)
    _processor->EvaluateRates(_k2, timeStepInDays, false);

    // Back to the start-of-step state
    _processor->ResetState();

    // Combined rate, constrained at the start-of-step state
    _combinedRates = (_k1 + _k2) / 2;
    _processor->ConstrainRates(_combinedRates, timeStepInDays);

    // Advance the state over the full step and commit
    _processor->ApplyRates(_combinedRates, timeStepInDays);
    _processor->FinalizeTimeStep();

    return true;
}
