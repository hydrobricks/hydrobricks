#include "Processor.h"

#include "ModelHydro.h"
#include "SubBasin.h"

Processor::Processor()
    : _solver(nullptr),
      _model(nullptr),
      _solvableConnectionCount(0),
      _directConnectionCount(0) {}

Processor::~Processor() {
    wxDELETE(_solver);
}

void Processor::Initialize(const SolverSettings& solverSettings) {
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

    for (int iUnit = 0; iUnit < basin->GetHydroUnitCount(); ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        bool solverRequired = false;
        for (int iBrick = 0; iBrick < unit->GetBricksCount(); ++iBrick) {
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

    for (int iBrick = 0; iBrick < basin->GetBricksCount(); ++iBrick) {
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

void Processor::StoreStateVariableChanges(vecDoublePt& values) {
    if (!values.empty()) {
        for (auto const& value : values) {
            _stateVariableChanges.push_back(value);
        }
    }
}

int Processor::GetNbStateVariables() {
    return static_cast<int>(_stateVariableChanges.size());
}

bool Processor::ProcessTimeStep() {
    wxASSERT(_model);

    SubBasin* basin = _model->GetSubBasin();

    // Process the bricks that do not need a solver.
    int ptIndex = 0;
    for (int iUnit = 0; iUnit < basin->GetHydroUnitCount(); ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        for (int iSplitter = 0; iSplitter < unit->GetSplittersCount(); ++iSplitter) {
            Splitter* splitter = unit->GetSplitter(iSplitter);
            splitter->Compute();
        }
        for (int iBrick = 0; iBrick < unit->GetBricksCount(); ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);
            if (brick->NeedsSolver()) {
                continue;
            }
            if (brick->IsNull()) {
                continue;
            }

            ApplyDirectChanges(brick, ptIndex);
        }
    }

    // Process the bricks that need a solver
    if (!_solver->Solve()) {
        return false;
    }

    if (!basin->ComputeOutletDischarge()) {
        return false;
    }

    return true;
}

void Processor::ApplyDirectChanges(Brick* brick, int& ptIndex) {
    brick->UpdateContentFromInputs();

    // Initialize the change rates to 0 and link to fluxes
    int iRate = ptIndex;
    for (auto process : brick->GetProcesses()) {
        for (int i = 0; i < process->GetOutputFluxCount(); ++i) {
            wxASSERT(_changeRatesNoSolver.rows() > iRate);
            _changeRatesNoSolver(iRate) = 0;

            // Link to fluxes to enforce subsequent constraints
            process->StoreInOutgoingFlux(&_changeRatesNoSolver(iRate), i);
            iRate++;
        }
    }

    iRate = ptIndex;
    for (auto process : brick->GetProcesses()) {
        // Get the change rates (per day) independently of the time step and constraints
        vecDouble rates = process->GetChangeRates();

        int iRateCopy = iRate;
        for (double rate : rates) {
            wxASSERT(_changeRatesNoSolver.rows() > iRateCopy);
            _changeRatesNoSolver(iRateCopy) = rate;
            iRateCopy++;
        }

        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        process->GetWaterContainer()->ApplyConstraints(config::timeStepInDays);

        // Apply changes
        for (int i = 0; i < rates.size(); ++i) {
            process->ApplyChange(i, _changeRatesNoSolver(iRate), config::timeStepInDays);
            _changeRatesNoSolver(iRate) = 0;
            iRate++;
            ptIndex++;
        }
    }

    brick->Finalize();
}
