#include "Processor.h"

#include "ModelHydro.h"
#include "SubBasin.h"

Processor::Processor()
    : m_solver(nullptr),
      m_model(nullptr),
      m_solvableConnectionsNb(0),
      m_directConnectionsNb(0) {}

Processor::~Processor() {
    wxDELETE(m_solver);
}

void Processor::Initialize(const SolverSettings& solverSettings) {
    m_solver = Solver::Factory(solverSettings);
    m_solver->Connect(this);
    ConnectToElementsToSolve();
    m_solver->InitializeContainers();
    m_changeRatesNoSolver = axd::Zero(m_directConnectionsNb);
}

void Processor::SetModel(ModelHydro* model) {
    m_model = model;
}

void Processor::ConnectToElementsToSolve() {
    SubBasin* basin = m_model->GetSubBasin();

    for (int iUnit = 0; iUnit < basin->GetHydroUnitsNb(); ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        bool solverRequired = false;
        for (int iBrick = 0; iBrick < unit->GetBricksCount(); ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);

            // Add the bricks that need a solver and all their children
            if (brick->NeedsSolver() || solverRequired) {
                m_iterableBricks.push_back(brick);
                solverRequired = true;

                // Get state variables from bricks
                vecDoublePt bricksValues = brick->GetStateVariableChanges();
                StoreStateVariableChanges(bricksValues);

                // Get state variables from processes
                vecDoublePt processValues = brick->GetStateVariableChangesFromProcesses();
                StoreStateVariableChanges(processValues);

                // Count connections
                m_solvableConnectionsNb += brick->GetProcessesConnectionsNb();
            } else {
                // Count connections
                m_directConnectionsNb += brick->GetProcessesConnectionsNb();
            }
        }
    }

    for (int iBrick = 0; iBrick < basin->GetBricksCount(); ++iBrick) {
        Brick* brick = basin->GetBrick(iBrick);

        // Add the bricks need a solver here
        m_iterableBricks.push_back(brick);

        // Get state variables from bricks
        vecDoublePt bricksValues = brick->GetStateVariableChanges();
        StoreStateVariableChanges(bricksValues);

        // Get state variables from processes
        vecDoublePt processValues = brick->GetStateVariableChangesFromProcesses();
        StoreStateVariableChanges(processValues);

        // Count connections
        m_solvableConnectionsNb += brick->GetProcessesConnectionsNb();
    }
}

void Processor::StoreStateVariableChanges(vecDoublePt& values) {
    if (!values.empty()) {
        for (auto const& value : values) {
            m_stateVariableChanges.push_back(value);
        }
    }
}

int Processor::GetNbStateVariables() {
    return int(m_stateVariableChanges.size());
}

bool Processor::ProcessTimeStep() {
    wxASSERT(m_model);

    SubBasin* basin = m_model->GetSubBasin();

    // Process the bricks that do not need a solver.
    for (int iUnit = 0; iUnit < basin->GetHydroUnitsNb(); ++iUnit) {
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

            ApplyDirectChanges(brick);
        }
    }

    // Process the bricks that need a solver
    if (!m_solver->Solve()) {
        return false;
    }

    if (!basin->ComputeOutletDischarge()) {
        return false;
    }

    return true;
}

void Processor::ApplyDirectChanges(Brick* brick) {
    brick->UpdateContentFromInputs();

    // Initialize the change rates to 0 and link to fluxes
    int iRate = 0;
    for (auto process : brick->GetProcesses()) {
        for (int i = 0; i < process->GetOutputFluxesNb(); ++i) {
            wxASSERT(m_changeRatesNoSolver.rows() > iRate);
            m_changeRatesNoSolver(iRate) = 0;

            // Link to fluxes to enforce subsequent constraints
            process->StoreInOutgoingFlux(&m_changeRatesNoSolver(iRate), i);
            iRate++;
        }
    }

    iRate = 0;
    for (auto process : brick->GetProcesses()) {
        // Get the change rates (per day) independently of the time step and constraints
        vecDouble rates = process->GetChangeRates();

        int iRateCopy = iRate;
        for (double rate : rates) {
            wxASSERT(m_changeRatesNoSolver.rows() > iRateCopy);
            m_changeRatesNoSolver(iRateCopy) = rate;
            iRateCopy++;
        }

        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        process->GetWaterContainer()->ApplyConstraints(g_timeStepInDays, false);

        // Apply changes
        for (int i = 0; i < rates.size(); ++i) {
            process->ApplyChange(i, m_changeRatesNoSolver(iRate), g_timeStepInDays);
            m_changeRatesNoSolver(iRate) = 0;
            iRate++;
        }
    }

    brick->Finalize();
}
