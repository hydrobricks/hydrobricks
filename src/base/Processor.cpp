#include "Processor.h"

#include "ModelHydro.h"
#include "SubBasin.h"

Processor::Processor(Solver* solver)
    : m_solver(solver),
      m_model(nullptr)
{
    m_solver->Connect(this);
}

void Processor::SetModel(ModelHydro* model) {
    m_model = model;
}

void Processor::Initialize() {
    ConnectToIterableBricks();
    ConnectToIterableValues();
    m_solver->InitializeContainers();
}

void Processor::ConnectToIterableBricks() {
    SubBasin* basin = m_model->GetSubBasin();

    int nUnits = basin->GetHydroUnitsCount();

    for (int iUnit = 0; iUnit < nUnits; ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        int nBricks = unit->GetBricksCount();
        bool solverRequired = false;

        for (int iBrick = 0; iBrick < nBricks; ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);

            // Add the bricks that need a solver and all their children
            if (brick->NeedsSolver() || solverRequired) {
                m_iterableBricks.push_back(brick);
                solverRequired = true;
            }
        }
    }
}

void Processor::ConnectToIterableValues() {
    SubBasin* basin = m_model->GetSubBasin();

    int nUnits = basin->GetHydroUnitsCount();

    for (int iUnit = 0; iUnit < nUnits; ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        int nBricks = unit->GetBricksCount();

        for (int iBrick = 0; iBrick < nBricks; ++iBrick) {
            // Get iterable values from bricks
            Brick* brick = unit->GetBrick(iBrick);
            std::vector<double*> bricksValues = brick->GetIterableValues();
            StoreIterableValues(bricksValues);

            // Get iterable values from processes
            std::vector<double*> processValues = brick->GetIterableValuesFromProcesses();
            StoreIterableValues(processValues);
        }
    }
}

void Processor::StoreIterableValues(std::vector<double*>& values) {
    if (!values.empty()) {
        for (auto const& value : values) {
            m_iterableValues.push_back(value);
        }
    }
}

int Processor::GetNbIterableValues() {
    return int(m_iterableValues.size());
}

bool Processor::ProcessTimeStep() {
    wxASSERT(m_model);

    SubBasin* basin = m_model->GetSubBasin();

    int nUnits = basin->GetHydroUnitsCount();

    // Process the bricks that do not need a solver.
    for (int iUnit = 0; iUnit < nUnits; ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        int nBricks = unit->GetBricksCount();

        for (int iBrick = 0; iBrick < nBricks; ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);
            brick->Compute();
        }
    }

    // Process the bricks that need a solver
    m_solver->Solve();

    return false;
}
