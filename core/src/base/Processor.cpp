#include "Processor.h"

#include "ModelHydro.h"
#include "SubBasin.h"

Processor::Processor()
    : _solver(nullptr),
      _model(nullptr),
      _solvableConnectionCount(0),
      _directConnectionCount(0) {}

Processor::~Processor() = default;  // Automatic cleanup via unique_ptr

void Processor::Initialize(const SolverSettings& solverSettings) {
    // Should not be called twice
    if (_solver) {
        throw std::runtime_error("Processor::Initialize - Processor is already initialized.");
    }

    _solver = Solver::Factory(solverSettings);
    _solver->Connect(this);
    ConnectToElementsToSolve();
    _solver->InitializeContainers();
    _changeRatesNoSolver = axd::Zero(_directConnectionCount);
}

void Processor::SetModel(ModelHydro* model) {
    _model = model;
}

void Processor::ConnectToElementsToSolve() {
    SubBasin* basin = _model->GetSubBasin();

    int hydroUnitCount = basin->GetHydroUnitCount();
    for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        bool solverRequired = false;
        int brickCount = unit->GetBrickCount();
        for (int iBrick = 0; iBrick < brickCount; ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);

            // Add the bricks that need a solver and all their children
            if (brick->NeedsSolver() || solverRequired) {
                _iterableBricks.push_back(brick);
                solverRequired = true;

                // Get state variables from bricks
                vecDoublePt bricksValues = brick->GetDynamicContentChanges();
                StoreStateVariableChanges(bricksValues);

                // Get state variables from processes
                vecDoublePt processValues = brick->GetStateVariableChangesFromProcesses();
                StoreStateVariableChanges(processValues);

                // Count connections
                _solvableConnectionCount += brick->GetProcessConnectionCount();
            } else {
                // Count connections
                _directConnectionCount += brick->GetProcessConnectionCount();
            }
        }
    }

    int basinBrickCount = basin->GetBrickCount();
    for (int iBrick = 0; iBrick < basinBrickCount; ++iBrick) {
        Brick* brick = basin->GetBrick(iBrick);

        // Add the bricks need a solver here
        _iterableBricks.push_back(brick);

        // Get state variables from bricks
        vecDoublePt bricksValues = brick->GetDynamicContentChanges();
        StoreStateVariableChanges(bricksValues);

        // Get state variables from processes
        vecDoublePt processValues = brick->GetStateVariableChangesFromProcesses();
        StoreStateVariableChanges(processValues);

        // Count connections
        _solvableConnectionCount += brick->GetProcessConnectionCount();
    }
}

void Processor::StoreStateVariableChanges(std::span<double*> values) {
    if (!values.empty()) {
        for (auto const& value : values) {
            _stateVariableChanges.push_back(value);
        }
    }
}

int Processor::GetStateVariableCount() const {
    return static_cast<int>(_stateVariableChanges.size());
}

void Processor::GatherState(axd& state) const {
    assert(state.size() == static_cast<int>(_stateVariableChanges.size()));
    for (auto [i, value] : std::views::enumerate(_stateVariableChanges)) {
        state(i) = *value;
    }
}

void Processor::ScatterState(const axd& state) {
    assert(state.size() == static_cast<int>(_stateVariableChanges.size()));
    for (auto [i, value] : std::views::enumerate(_stateVariableChanges)) {
        *value = state(i);
    }
}

void Processor::ResetState() {
    for (auto value : _stateVariableChanges) {
        *value = 0;
    }
}

void Processor::EvaluateRates(axd& rates, double timeStepInDays, bool applyConstraints) {
    int iRate = 0;
    for (auto brick : _iterableBricks) {
        double sumRates = 0.0;
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);

            // Get the change rates (per day) independently of the time step and constraints (null bricks handled).
            // Reference into the process's reusable buffer; consumed below before the next process is queried.
            const vecDouble& processRates = process->GetChangeRates();

            for (int j = 0; j < processRates.size(); ++j) {
                assert(rates.size() > iRate);
                rates(iRate) = processRates[j];
                sumRates += processRates[j];

                // Link to fluxes to enforce subsequent constraints
                if (applyConstraints) {
                    process->StoreInOutgoingFlux(&rates(iRate), j);
                }
                iRate++;
            }
        }

        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        if (applyConstraints && GreaterThan(sumRates, 0, PRECISION)) {
            brick->ApplyConstraints(timeStepInDays);
        }
    }
}

void Processor::ConstrainRates(axd& rates, double timeStepInDays) {
    int iRate = 0;
    for (auto brick : _iterableBricks) {
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);
            for (int j = 0; j < process->GetConnectionCount(); ++j) {
                assert(rates.size() > iRate);
                // Link to fluxes to enforce subsequent constraints
                process->StoreInOutgoingFlux(&rates(iRate), j);
                iRate++;
            }
        }
        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        brick->ApplyConstraints(timeStepInDays);
    }
}

void Processor::ApplyRates(const axd& rates, double timeStepInDays) {
    int iRate = 0;
    for (auto brick : _iterableBricks) {
        if (brick->IsNull()) {
            continue;
        }
        brick->UpdateContentFromInputs();
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);
            for (int iConnect = 0; iConnect < process->GetConnectionCount(); ++iConnect) {
                process->ApplyChange(iConnect, rates(iRate), timeStepInDays);
                iRate++;
            }
        }
    }
}

void Processor::FinalizeTimeStep() {
    for (auto brick : _iterableBricks) {
        if (brick->IsNull()) {
            continue;
        }
        brick->Finalize();
    }
}

bool Processor::ProcessTimeStep(double timeStepInDays) {
    assert(_model);

    SubBasin* basin = _model->GetSubBasin();

    // Process the bricks that do not need a solver.
    int ptIndex = 0;
    int hydroUnitCount = basin->GetHydroUnitCount();
    for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        int splitterCount = unit->GetSplitterCount();
        for (int iSplitter = 0; iSplitter < splitterCount; ++iSplitter) {
            Splitter* splitter = unit->GetSplitter(iSplitter);
            splitter->Compute();
        }
        int brickCount = unit->GetBrickCount();
        for (int iBrick = 0; iBrick < brickCount; ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);
            if (brick->NeedsSolver()) {
                continue;
            }
            if (brick->IsNull()) {
                continue;
            }

            ApplyDirectChanges(brick, ptIndex, timeStepInDays);
        }
    }

    // Process the bricks that need a solver
    if (!_solver->Solve(timeStepInDays)) {
        return false;
    }

    if (!basin->ComputeOutletDischarge()) {
        return false;
    }

    return true;
}

void Processor::ApplyDirectChanges(Brick* brick, int& ptIndex, double timeStepInDays) {
    brick->UpdateContentFromInputs();

    // Initialize the change rates to 0 and link to fluxes
    int iRate = ptIndex;
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        auto process = brick->GetProcess(i);
        for (int j = 0; j < process->GetOutputFluxCount(); ++j) {
            assert(_changeRatesNoSolver.rows() > iRate);
            _changeRatesNoSolver(iRate) = 0;

            // Link to fluxes to enforce subsequent constraints
            process->StoreInOutgoingFlux(&_changeRatesNoSolver(iRate), j);
            iRate++;
        }
    }

    iRate = ptIndex;
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        auto process = brick->GetProcess(i);

        // Get the change rates (per day) independently of the time step and constraints.
        // Reference into the process's reusable buffer; consumed within this iteration.
        const vecDouble& rates = process->GetChangeRates();

        int iRateCopy = iRate;
        for (double rate : rates) {
            assert(_changeRatesNoSolver.rows() > iRateCopy);
            _changeRatesNoSolver(iRateCopy) = rate;
            iRateCopy++;
        }

        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        process->GetWaterContainer()->ApplyConstraints(timeStepInDays);

        // Apply changes
        for (int j = 0; j < rates.size(); ++j) {
            process->ApplyChange(j, _changeRatesNoSolver(iRate), timeStepInDays);
            _changeRatesNoSolver(iRate) = 0;
            iRate++;
            ptIndex++;
        }
    }

    brick->Finalize();
}
