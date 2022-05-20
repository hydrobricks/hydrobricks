#include "Processor.h"

#include "ModelHydro.h"
#include "SubBasin.h"

Processor::Processor()
    : m_solver(nullptr),
      m_model(nullptr),
      m_solvableConnectionsNb(0),
      m_directConnectionsNb(0)
{
}

Processor::~Processor() {
    wxDELETE(m_solver);
}

void Processor::Initialize(const SolverSettings &solverSettings) {
    m_solver = Solver::Factory(solverSettings);
    m_solver->Connect(this);
    ConnectToElementsToSolve();
    m_solver->InitializeContainers();
    m_solver->SetTimeStepInDays(m_model->GetTimeMachine()->GetTimeStepPointer());
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
    int iRate = 0;
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

            ApplyDirectChanges(brick, iRate);
        }
    }

    // Process the bricks that need a solver
    if (!m_solver->Solve()) {
        return false;
    }

    if (!basin->ComputeAggregatedValues()) {
        return false;
    }

    return true;
}

void Processor::ApplyDirectChanges(Brick* brick, int &iRate) {
    brick->UpdateContentFromInputs();

    for (auto process : brick->GetProcesses()) {
        // Get the change rates (per day) independently of the time step and constraints
        vecDouble rates = process->GetChangeRates();

        for (int i = 0; i < rates.size(); ++i) {
            wxASSERT(m_changeRatesNoSolver.rows() > iRate);
            m_changeRatesNoSolver(iRate + i) = rates[i];

            // Link to fluxes to enforce subsequent constraints
            process->StoreInOutgoingFlux(&m_changeRatesNoSolver(iRate + i), i);
        }

        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        brick->ApplyConstraints(*m_model->GetTimeMachine()->GetTimeStepPointer());

        for (int i = 0; i < rates.size(); ++i) {
            process->ApplyChange(i, m_changeRatesNoSolver(iRate + i), *m_model->GetTimeMachine()->GetTimeStepPointer());
            iRate++;
        }
    }

    brick->Finalize();
}
