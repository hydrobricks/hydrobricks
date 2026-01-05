#include "Solver.h"

#include "Processor.h"
#include "SolverEulerExplicit.h"
#include "SolverHeunExplicit.h"
#include "SolverRK4.h"

Solver::Solver()
    : _processor(nullptr),
      _nIterations(1) {}

Solver* Solver::Factory(const SolverSettings& solverSettings) {
    if (solverSettings.name == "rk4" || solverSettings.name == "runge_kutta") {
        return new SolverRK4();
    } else if (solverSettings.name == "euler_explicit") {
        return new SolverEulerExplicit();
    } else if (solverSettings.name == "heun_explicit") {
        return new SolverHeunExplicit();
    }
    throw InvalidArgument(wxString::Format(_("Incorrect solver name: %s."), solverSettings.name));
}

void Solver::InitializeContainers() {
    wxASSERT(_processor);
    wxASSERT(_nIterations > 0);
    _stateVariableChanges = axxd::Zero(_processor->GetNbStateVariables(), _nIterations);
    _changeRates = axxd::Zero(_processor->GetNbSolvableConnections(), _nIterations);
}

void Solver::SaveStateVariables(int col) {
    wxASSERT(_processor);
    int counter = 0;
    for (auto value : *(_processor->GetStateVariablesVectorPt())) {
        _stateVariableChanges(counter, col) = *value;
        counter++;
    }
}

void Solver::ComputeChangeRates(int col, bool applyConstraints) {
    wxASSERT(_processor);
    int iRate = 0;
    for (auto brick : *(_processor->GetIterableBricksVectorPt())) {
        double sumRates = 0.0;
        for (auto process : brick->GetProcesses()) {
            // Get the change rates (per day) independently of the time step and constraints (null bricks handled)
            vecDouble rates = process->GetChangeRates();

            for (int i = 0; i < rates.size(); ++i) {
                wxASSERT(_changeRates.rows() > iRate);
                _changeRates(iRate, col) = rates[i];
                sumRates += rates[i];

                // Link to fluxes to enforce subsequent constraints
                if (applyConstraints) {
                    process->StoreInOutgoingFlux(&_changeRates(iRate, col), i);
                }
                iRate++;
            }
        }

        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        if (applyConstraints && sumRates > PRECISION) {
            brick->ApplyConstraints(config::timeStepInDays);
        }
    }
}

void Solver::ApplyConstraintsFor(int col) {
    wxASSERT(_processor);
    int iRate = 0;
    for (auto brick : *(_processor->GetIterableBricksVectorPt())) {
        for (auto process : brick->GetProcesses()) {
            for (int i = 0; i < process->GetConnectionCount(); ++i) {
                wxASSERT(_changeRates.rows() > iRate);
                // Link to fluxes to enforce subsequent constraints
                process->StoreInOutgoingFlux(&_changeRates(iRate, col), i);
                iRate++;
            }
        }
        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        brick->ApplyConstraints(config::timeStepInDays);
    }
}

void Solver::ResetStateVariableChanges() {
    wxASSERT(_processor);
    for (auto value : *(_processor->GetStateVariablesVectorPt())) {
        *value = 0;
    }
}

void Solver::SetStateVariablesToIteration(int col) {
    wxASSERT(_processor);
    int counter = 0;
    for (auto value : *(_processor->GetStateVariablesVectorPt())) {
        *value = _stateVariableChanges(counter, col);
        counter++;
    }
}

void Solver::SetStateVariablesToAvgOf(int col1, int col2) {
    wxASSERT(_processor);
    int counter = 0;
    for (auto value : *(_processor->GetStateVariablesVectorPt())) {
        *value = (_stateVariableChanges(counter, col1) + _stateVariableChanges(counter, col2)) / 2.0;
        counter++;
    }
}

void Solver::ApplyProcesses(int col) const {
    wxASSERT(_processor);
    int iRate = 0;
    for (auto brick : *(_processor->GetIterableBricksVectorPt())) {
        if (brick->IsNull()) {
            continue;
        }
        brick->UpdateContentFromInputs();
        for (auto process : brick->GetProcesses()) {
            for (int iConnect = 0; iConnect < process->GetConnectionCount(); ++iConnect) {
                process->ApplyChange(iConnect, _changeRates(iRate, col), config::timeStepInDays);
                iRate++;
            }
        }
    }
}

void Solver::ApplyProcesses(const axd& changeRates) const {
    wxASSERT(_processor);
    int iRate = 0;
    for (auto brick : *(_processor->GetIterableBricksVectorPt())) {
        if (brick->IsNull()) {
            continue;
        }
        brick->UpdateContentFromInputs();
        for (auto process : brick->GetProcesses()) {
            for (int iConnect = 0; iConnect < process->GetConnectionCount(); ++iConnect) {
                process->ApplyChange(iConnect, changeRates(iRate), config::timeStepInDays);
                iRate++;
            }
        }
    }
}

void Solver::Finalize() const {
    wxASSERT(_processor);
    for (auto brick : *(_processor->GetIterableBricksVectorPt())) {
        if (brick->IsNull()) {
            continue;
        }
        brick->Finalize();
        for (auto process : brick->GetProcesses()) {
            process->Finalize();
        }
    }
}
