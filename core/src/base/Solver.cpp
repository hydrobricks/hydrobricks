#include "Solver.h"

#include <functional>
#include <unordered_map>
#include <vector>

#include "Processor.h"
#include "SolverEulerExplicit.h"
#include "SolverHeunExplicit.h"
#include "SolverRK4.h"

Solver::Solver()
    : _processor(nullptr),
      _nIterations(1) {}

static string GetValidSolverNames() {
    static const vector<string> validNames = {"rk4", "runge_kutta",  // Synonyms for RK4
                                              "euler_explicit", "heun_explicit"};

    string suggestions = "Valid solver names: ";
    for (size_t i = 0; i < validNames.size(); ++i) {
        suggestions += validNames[i];
        if (i < validNames.size() - 1) {
            suggestions += ", ";
        }
    }
    return suggestions;
}

std::unique_ptr<Solver> Solver::Factory(const SolverSettings& solverSettings) {
    using FactoryFunc = std::function<std::unique_ptr<Solver>()>;

    static const std::unordered_map<string, FactoryFunc> factoryMap = {
        {"rk4", []() { return std::make_unique<SolverRK4>(); }},
        {"runge_kutta", []() { return std::make_unique<SolverRK4>(); }},
        {"euler_explicit", []() { return std::make_unique<SolverEulerExplicit>(); }},
        {"heun_explicit", []() { return std::make_unique<SolverHeunExplicit>(); }}};

    auto it = factoryMap.find(solverSettings.name);
    if (it != factoryMap.end()) {
        return it->second();
    }

    throw ModelConfigError(std::format("Incorrect solver name: {}. {}", solverSettings.name, GetValidSolverNames()));
}

void Solver::InitializeContainers() {
    assert(_processor);
    assert(_nIterations > 0);
    _stateVariableChanges = axxd::Zero(_processor->GetStateVariableCount(), _nIterations);
    _changeRates = axxd::Zero(_processor->GetSolvableConnectionCount(), _nIterations);
}

void Solver::SaveStateVariables(int col) {
    assert(_processor);
    for (auto [i, value] : std::views::enumerate(_processor->GetStateVariables())) {
        _stateVariableChanges(i, col) = *value;
    }
}

void Solver::ComputeChangeRates(int col, bool applyConstraints) {
    assert(_processor);
    int iRate = 0;
    for (auto brick : *(_processor->GetIterableBricksVectorPt())) {
        double sumRates = 0.0;
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);

            // Get the change rates (per day) independently of the time step and constraints (null bricks handled).
            // Reference into the process's reusable buffer; consumed below before the next process is queried.
            const vecDouble& rates = process->GetChangeRates();

            for (int j = 0; j < rates.size(); ++j) {
                assert(_changeRates.rows() > iRate);
                _changeRates(iRate, col) = rates[j];
                sumRates += rates[j];

                // Link to fluxes to enforce subsequent constraints
                if (applyConstraints) {
                    process->StoreInOutgoingFlux(&_changeRates(iRate, col), j);
                }
                iRate++;
            }
        }

        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        if (applyConstraints && GreaterThan(sumRates, 0, PRECISION)) {
            brick->ApplyConstraints(_timeStepInDays);
        }
    }
}

void Solver::ApplyConstraintsFor(int col) {
    assert(_processor);
    int iRate = 0;
    for (auto brick : *(_processor->GetIterableBricksVectorPt())) {
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);
            for (int j = 0; j < process->GetConnectionCount(); ++j) {
                assert(_changeRates.rows() > iRate);
                // Link to fluxes to enforce subsequent constraints
                process->StoreInOutgoingFlux(&_changeRates(iRate, col), j);
                iRate++;
            }
        }
        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        brick->ApplyConstraints(_timeStepInDays);
    }
}

void Solver::ResetStateVariableChanges() {
    assert(_processor);
    for (auto value : _processor->GetStateVariables()) {
        *value = 0;
    }
}

void Solver::SetStateVariablesToIteration(int col) {
    assert(_processor);
    for (auto [i, value] : std::views::enumerate(_processor->GetStateVariables())) {
        *value = _stateVariableChanges(i, col);
    }
}

void Solver::SetStateVariablesToAvgOf(int col1, int col2) {
    assert(_processor);
    for (auto [i, value] : std::views::enumerate(_processor->GetStateVariables())) {
        *value = (_stateVariableChanges(i, col1) + _stateVariableChanges(i, col2)) / 2.0;
    }
}

void Solver::ApplyProcesses(int col) const {
    assert(_processor);
    int iRate = 0;
    for (auto brick : *(_processor->GetIterableBricksVectorPt())) {
        if (brick->IsNull()) {
            continue;
        }
        brick->UpdateContentFromInputs();
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);
            for (int iConnect = 0; iConnect < process->GetConnectionCount(); ++iConnect) {
                process->ApplyChange(iConnect, _changeRates(iRate, col), _timeStepInDays);
                iRate++;
            }
        }
    }
}

void Solver::ApplyProcesses(const axd& changeRates) const {
    assert(_processor);
    int iRate = 0;
    for (auto brick : *(_processor->GetIterableBricksVectorPt())) {
        if (brick->IsNull()) {
            continue;
        }
        brick->UpdateContentFromInputs();
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);
            for (int iConnect = 0; iConnect < process->GetConnectionCount(); ++iConnect) {
                process->ApplyChange(iConnect, changeRates(iRate), _timeStepInDays);
                iRate++;
            }
        }
    }
}

void Solver::Finalize() const {
    assert(_processor);
    for (auto brick : *(_processor->GetIterableBricksVectorPt())) {
        if (brick->IsNull()) {
            continue;
        }
        brick->Finalize();
    }
}
