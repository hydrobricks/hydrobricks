#include "Solver.h"

#include "Processor.h"
#include "SolverEulerExplicit.h"
#include "SolverHeunExplicit.h"
#include "SolverRK4.h"

Solver::Solver()
    : m_processor(nullptr),
      m_nIterations(1)
{}

Solver* Solver::Factory(const SolverSettings &solverSettings) {
    if (solverSettings.name.IsSameAs("RK4", false) ||
        solverSettings.name.IsSameAs("Runge-Kutta", false) ||
        solverSettings.name.IsSameAs("RungeKutta", false)) {
        return new SolverRK4();
    } else if (solverSettings.name.IsSameAs("EulerExplicit", false) ||
               solverSettings.name.IsSameAs("Euler Explicit", false)) {
        return new SolverEulerExplicit();
    } else if (solverSettings.name.IsSameAs("HeunExplicit", false) ||
               solverSettings.name.IsSameAs("Heun Explicit", false)) {
        return new SolverHeunExplicit();
    }
    throw InvalidArgument(_("Incorrect solver name."));
}

void Solver::InitializeContainers() {
    m_stateVariables.resize(m_processor->GetNbStateVariables(), m_nIterations);
    m_changeRates.resize(m_processor->GetNbConnections(), m_nIterations);
}


void Solver::SaveStateVariables(int col) {
    int counter = 0;
    for (auto value : *(m_processor->GetStateVariablesVectorPt())) {
        m_stateVariables(counter, col) = *value;
        counter++;
    }
}

void Solver::ComputeChangeRates(int col) {
    int iRate = 0;
    for (auto brick : *(m_processor->GetIterableBricksVectorPt())) {
        for (auto process : brick->GetProcesses()) {
            vecDouble rates = process->GetChangeRates();
            for (double rate : rates) {
                wxASSERT(m_changeRates.rows() > iRate);
                m_changeRates(iRate, col) = rate;
                iRate++;
            }
        }
    }
}

void Solver::SetStateVariablesToIteration(int col) {
    int counter = 0;
    for (auto value: *(m_processor->GetStateVariablesVectorPt())) {
        *value = m_stateVariables(counter, col);
        counter++;
    }
}

void Solver::SetStateVariablesToAvgOf(int col1, int col2) {
    int counter = 0;
    for (auto value: *(m_processor->GetStateVariablesVectorPt())) {
        *value = (m_stateVariables(counter, col1) + m_stateVariables(counter, col2)) / 2;
        counter++;
    }
}

void Solver::ApplyProcesses(int col) const {
    int iRate = 0;
    for (auto brick : *(m_processor->GetIterableBricksVectorPt())) {
        brick->UpdateContentFromInputs();
        for (auto process : brick->GetProcesses()) {
            for (int iConnect = 0; iConnect < process->GetConnectionsNb(); ++iConnect) {
                process->ApplyChange(iConnect, m_changeRates(iRate, col), *m_timeStepInDays);
                iRate++;
            }
        }
    }
}

void Solver::ApplyProcesses(const axd &changeRates) const {
    int iRate = 0;
    for (auto brick : *(m_processor->GetIterableBricksVectorPt())) {
        brick->UpdateContentFromInputs();
        for (auto process : brick->GetProcesses()) {
            for (int iConnect = 0; iConnect < process->GetConnectionsNb(); ++iConnect) {
                process->ApplyChange(iConnect, changeRates(iRate), *m_timeStepInDays);
                iRate++;
            }
        }
    }
}