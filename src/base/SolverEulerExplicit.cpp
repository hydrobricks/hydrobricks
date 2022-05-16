#include "Processor.h"
#include "SolverEulerExplicit.h"

SolverEulerExplicit::SolverEulerExplicit()
    : Solver()
{
    m_nIterations = 1;
}

bool SolverEulerExplicit::Solve() {

    // Compute the change rates
    ComputeChangeRates(0);

    // Apply the changes
    ApplyProcesses(0);
    Finalize();
    
    return true;
}