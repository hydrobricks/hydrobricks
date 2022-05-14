#include "SolverHeunExplicit.h"

#include "Processor.h"

SolverHeunExplicit::SolverHeunExplicit()
    : Solver()
{
    m_nIterations = 3;
}

bool SolverHeunExplicit::Solve() {

    // Save the original state variables
    SaveStateVariables(0);

    // Compute the change rates for k1 = f(tn, Sn)
    ComputeChangeRates(0);

    // Apply the changes
    ApplyProcesses(0);

    // Compute the change rates for k1 = f(tn + h, Sn + k1 h)
    ComputeChangeRates(1, false);

    // Restore original state variables
    SetStateVariablesToIteration(0);

    // Final change rates
    m_changeRates.col(2) = (m_changeRates.col(0) + m_changeRates.col(1)) / 2;

    // Apply the changes
    ApplyConstraintsFor(2);
    ApplyProcesses(m_changeRates.col(2));

    return true;
}