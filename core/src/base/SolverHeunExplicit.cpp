#include "SolverHeunExplicit.h"

#include "Processor.h"

SolverHeunExplicit::SolverHeunExplicit()
    : Solver() {
    _nIterations = 3;
}

bool SolverHeunExplicit::Solve() {
    // Compute the change rates for f(tn, Sn)
    ComputeChangeRates(0);

    // Apply the changes
    ApplyProcesses(0);

    // Compute the change rates for f(tn + h, Sn + k1 h)
    ComputeChangeRates(1, false);

    // Reset state variable changes to 0
    ResetStateVariableChanges();

    // Final change rates
    _changeRates.col(2) = (_changeRates.col(0) + _changeRates.col(1)) / 2;

    // Apply the changes
    ApplyConstraintsFor(2);
    ApplyProcesses(_changeRates.col(2));
    Finalize();

    return true;
}
