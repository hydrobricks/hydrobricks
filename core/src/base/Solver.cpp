#include "Solver.h"

#include "Processor.h"
#include "SolverEulerExplicit.h"
#include "SolverHeunExplicit.h"
#include "SolverRK4.h"

Solver::Solver()
    : m_processor(nullptr),
      m_nIterations(1) {}

Solver* Solver::Factory(const SolverSettings& solverSettings) {
    if (solverSettings.name == "RK4" || solverSettings.name == "Runge-Kutta" || solverSettings.name == "RungeKutta") {
        return new SolverRK4();
    } else if (solverSettings.name == "Euler Explicit" || solverSettings.name == "EulerExplicit") {
        return new SolverEulerExplicit();
    } else if (solverSettings.name == "Heun Explicit" || solverSettings.name == "HeunExplicit") {
        return new SolverHeunExplicit();
    }
    throw InvalidArgument(_("Incorrect solver name."));
}

void Solver::InitializeContainers() {
    wxASSERT(m_processor);
    wxASSERT(m_nIterations > 0);
    m_stateVariableChanges = axxd::Zero(m_processor->GetNbStateVariables(), m_nIterations);
    m_changeRates = axxd::Zero(m_processor->GetNbSolvableConnections(), m_nIterations);
}

void Solver::SaveStateVariables(int col) {
    wxASSERT(m_processor);
    int counter = 0;
    for (auto value : *(m_processor->GetStateVariablesVectorPt())) {
        m_stateVariableChanges(counter, col) = *value;
        counter++;
    }
}

void Solver::ComputeChangeRates(int col, bool applyConstraints) {
    wxASSERT(m_processor);
    int iRate = 0;
    for (auto brick : *(m_processor->GetIterableBricksVectorPt())) {
        double sumRates = 0.0;
        for (auto process : brick->GetProcesses()) {
            // Get the change rates (per day) independently of the time step and constraints (null bricks handled)
            vecDouble rates = process->GetChangeRates();

            for (int i = 0; i < rates.size(); ++i) {
                wxASSERT(m_changeRates.rows() > iRate);
                m_changeRates(iRate, col) = rates[i];
                sumRates += rates[i];

                // Link to fluxes to enforce subsequent constraints
                if (applyConstraints) {
                    process->StoreInOutgoingFlux(&m_changeRates(iRate, col), i);
                }
                iRate++;
            }
        }

        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        if (applyConstraints && sumRates > PRECISION) {
            brick->ApplyConstraints(g_timeStepInDays);
        }
    }
}

void Solver::ApplyConstraintsFor(int col) {
    wxASSERT(m_processor);
    int iRate = 0;
    for (auto brick : *(m_processor->GetIterableBricksVectorPt())) {
        for (auto process : brick->GetProcesses()) {
            for (int i = 0; i < process->GetConnectionsNb(); ++i) {
                wxASSERT(m_changeRates.rows() > iRate);
                // Link to fluxes to enforce subsequent constraints
                process->StoreInOutgoingFlux(&m_changeRates(iRate, col), i);
                iRate++;
            }
        }
        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        brick->ApplyConstraints(g_timeStepInDays);
    }
}

void Solver::ResetStateVariableChanges() {
    wxASSERT(m_processor);
    for (auto value : *(m_processor->GetStateVariablesVectorPt())) {
        *value = 0;
    }
}

void Solver::SetStateVariablesToIteration(int col) {
    wxASSERT(m_processor);
    int counter = 0;
    for (auto value : *(m_processor->GetStateVariablesVectorPt())) {
        *value = m_stateVariableChanges(counter, col);
        counter++;
    }
}

void Solver::SetStateVariablesToAvgOf(int col1, int col2) {
    wxASSERT(m_processor);
    int counter = 0;
    for (auto value : *(m_processor->GetStateVariablesVectorPt())) {
        *value = (m_stateVariableChanges(counter, col1) + m_stateVariableChanges(counter, col2)) / 2.0;
        counter++;
    }
}

void Solver::ApplyProcesses(int col) const {
    wxASSERT(m_processor);
    int iRate = 0;
    for (auto brick : *(m_processor->GetIterableBricksVectorPt())) {
        if (brick->IsNull()) {
            continue;
        }
        brick->UpdateContentFromInputs();
        for (auto process : brick->GetProcesses()) {
            for (int iConnect = 0; iConnect < process->GetConnectionsNb(); ++iConnect) {
                process->ApplyChange(iConnect, m_changeRates(iRate, col), g_timeStepInDays);
                iRate++;
            }
        }
    }
}

void Solver::ApplyProcesses(const axd& changeRates) const {
    wxASSERT(m_processor);
    int iRate = 0;
    for (auto brick : *(m_processor->GetIterableBricksVectorPt())) {
        if (brick->IsNull()) {
            continue;
        }
        brick->UpdateContentFromInputs();
        for (auto process : brick->GetProcesses()) {
            for (int iConnect = 0; iConnect < process->GetConnectionsNb(); ++iConnect) {
                process->ApplyChange(iConnect, changeRates(iRate), g_timeStepInDays);
                iRate++;
            }
        }
    }
}

void Solver::Finalize() const {
    wxASSERT(m_processor);
    for (auto brick : *(m_processor->GetIterableBricksVectorPt())) {
        if (brick->IsNull()) {
            continue;
        }
        brick->Finalize();
        for (auto process : brick->GetProcesses()) {
            process->Finalize();
        }
    }
}