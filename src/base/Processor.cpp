#include "Processor.h"

#include "ModelHydro.h"
#include "SubBasin.h"

Processor::Processor()
    : m_solver(nullptr),
      m_model(nullptr),
      m_connectionsNb(0)
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
                m_connectionsNb += brick->GetProcessesConnectionsNb();
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
    for (int iUnit = 0; iUnit < basin->GetHydroUnitsNb(); ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        for (int iBrick = 0; iBrick < unit->GetBricksCount(); ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);
            if (brick->NeedsSolver()) {
                continue;
            }

            throw NotImplemented();
            /*
            if (!brick->Compute()) {
                return false;
            }*/
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
