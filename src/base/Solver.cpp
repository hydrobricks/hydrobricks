#include "Solver.h"

#include "Processor.h"
#include "SolverExplicitEuler.h"
#include "SolverExplicitHeun.h"
#include "SolverRK4.h"

Solver::Solver()
    : m_processor(nullptr),
      m_nIterations(1)
{}

Solver* Solver::Factory(const SolverSettings &solverSettings) {
    if (solverSettings.name.IsSameAs("RK4", false)) {
        return new SolverRK4();
    } else if (solverSettings.name.IsSameAs("ExplicitEuler", false)) {
        return new SolverExplicitEuler();
    } else if (solverSettings.name.IsSameAs("ExplicitHeun", false)) {
        return new SolverExplicitHeun();
    }
    throw InvalidArgument(_("Incorrect solver name."));
}

void Solver::InitializeContainers() {
    m_container.resize(m_processor->GetNbIterableValues(), m_nIterations);
}