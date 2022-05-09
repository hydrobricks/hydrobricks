#include "Solver.h"

#include "Processor.h"
#include "SolverEulerExplicit.h"
#include "SolverExplicitHeun.h"
#include "SolverRK4.h"

Solver::Solver()
    : m_processor(nullptr),
      m_nIterations(1)
{}

Solver* Solver::Factory(const SolverSettings &solverSettings) {
    if (solverSettings.name.IsSameAs("RK4", false)) {
        return new SolverRK4();
    } else if (solverSettings.name.IsSameAs("EulerExplicit", false)) {
        return new SolverEulerExplicit();
    } else if (solverSettings.name.IsSameAs("ExplicitHeun", false)) {
        return new SolverExplicitHeun();
    }
    throw InvalidArgument(_("Incorrect solver name."));
}

void Solver::InitializeContainers() {
    m_stateVariables.resize(m_processor->GetNbIterableValues(), m_nIterations);
    m_changeRates.resize(m_processor->GetNbIterableValues(), m_nIterations);
}