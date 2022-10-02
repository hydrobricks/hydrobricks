#include "SolverHeunExplicit.h"

#include "Processor.h"

SolverHeunExplicit::SolverHeunExplicit() : Solver() {
    m_nIterations = 3;
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
    m_changeRates.col(2) = (m_changeRates.col(0) + m_changeRates.col(1)) / 2;

    // Apply the changes
    ApplyConstraintsFor(2);
    ApplyProcesses(m_changeRates.col(2));
    Finalize();

    return true;
}
