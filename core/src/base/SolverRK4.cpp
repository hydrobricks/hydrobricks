#include "SolverRK4.h"

#include "Processor.h"

SolverRK4::SolverRK4()
    : Solver() {}

void SolverRK4::InitializeContainers() {
    assert(_processor);
    int rateCount = _processor->GetSolvableConnectionCount();
    _k1 = axd::Zero(rateCount);
    _k2 = axd::Zero(rateCount);
    _k3 = axd::Zero(rateCount);
    _k4 = axd::Zero(rateCount);
    _combinedRates = axd::Zero(rateCount);
    _state = axd::Zero(_processor->GetStateVariableCount());
}

bool SolverRK4::Solve(double timeStepInDays) {
    // k1 = f(tn, Sn), with constraints
    _processor->EvaluateRates(_k1, timeStepInDays);

    // Advance the state to Sn + k1 h, then halve the changes to get the state at tn + h/2
    _processor->ApplyRates(_k1, timeStepInDays);
    _processor->GatherState(_state);
    _state *= 0.5;
    _processor->ScatterState(_state);

    // k2 = f(tn + h/2, Sn + k1 h/2), constrained at the start-of-step state
    _processor->EvaluateRates(_k2, timeStepInDays, false);
    _processor->ResetState();
    _processor->ConstrainRates(_k2, timeStepInDays);

    // Advance the state to Sn + k2 h, then halve the changes to get the state at tn + h/2
    _processor->ApplyRates(_k2, timeStepInDays);
    _processor->GatherState(_state);
    _state *= 0.5;
    _processor->ScatterState(_state);

    // k3 = f(tn + h/2, Sn + k2 h/2), constrained at the start-of-step state
    _processor->EvaluateRates(_k3, timeStepInDays, false);
    _processor->ResetState();
    _processor->ConstrainRates(_k3, timeStepInDays);

    // Advance the state to Sn + k3 h (full step)
    _processor->ApplyRates(_k3, timeStepInDays);

    // k4 = f(tn + h, Sn + k3 h)
    _processor->EvaluateRates(_k4, timeStepInDays, false);

    // Back to the start-of-step state
    _processor->ResetState();

    // Combined RK4 rate, constrained at the start-of-step state
    _combinedRates = (_k1 + 2 * _k2 + 2 * _k3 + _k4) / 6;
    _processor->ConstrainRates(_combinedRates, timeStepInDays);

    // Advance the state over the full step and commit
    _processor->ApplyRates(_combinedRates, timeStepInDays);
    _processor->FinalizeTimeStep();

    return true;
}
