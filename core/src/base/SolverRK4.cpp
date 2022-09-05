#include "Processor.h"
#include "SolverRK4.h"

SolverRK4::SolverRK4()
    : Solver()
{
    m_nIterations = 5;
}

bool SolverRK4::Solve() {

    // Compute the change rates for k1 = f(tn, Sn)
    ComputeChangeRates(0);

    // Apply the changes
    ApplyProcesses(0);

    // Save the new state variables
    SaveStateVariables(1);

    // Apply state variables for k1 at tn + h/2
    SetStateVariablesToAvgOf(0, 1);

    // Compute the change rates for k2 = f(tn + h/2, Sn + k1 h/2)
    ComputeChangeRates(1, false);

    // Reset state variable changes to 0
    ResetStateVariableChanges();

    // Apply the changes
    ApplyConstraintsFor(1);
    ApplyProcesses(1);

    // Save the new state variables
    SaveStateVariables(2);

    // Apply state variables for k2 at tn + h/2
    SetStateVariablesToAvgOf(0, 2);

    // Compute the change rates for k3 = f(tn + h/2, Sn + k2 h/2)
    ComputeChangeRates(2, false);

    // Reset state variable changes to 0
    ResetStateVariableChanges();

    // Apply the changes
    ApplyConstraintsFor(2);
    ApplyProcesses(2);

    // Save the new state variables
    SaveStateVariables(3);

    // Apply state variables for k3 at tn + h
    SetStateVariablesToIteration(3);

    // Compute the change rates for k4 = f(tn + h, Sn + k3 h)
    ComputeChangeRates(3, false);

    // Reset state variable changes to 0
    ResetStateVariableChanges();

    // Final change rate
    m_changeRates.col(4) = (m_changeRates.col(0) + 2 * m_changeRates.col(1) +
                            2 * m_changeRates.col(2) + m_changeRates.col(3)) / 6;

    // Apply the final rates
    ApplyConstraintsFor(4);
    ApplyProcesses(m_changeRates.col(4));
    Finalize();

    return true;
}


