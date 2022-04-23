#include "Processor.h"

#include "ModelHydro.h"
#include "SubBasin.h"

Processor::Processor(Solver* solver)
    : m_solver(solver),
      m_model(nullptr)
{}

void Processor::SetModel(ModelHydro* model) {
    m_model = model;
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

            // Get iterable values from fluxes
            std::vector<double*> fluxValues = brick->GetIterableValuesFromOutgoingFluxes();
            StoreIterableValues(fluxValues);
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

bool Processor::ProcessTimeStep() {
    wxASSERT(m_model);

    SubBasin* basin = m_model->GetSubBasin();

    int nUnits = basin->GetHydroUnitsCount();

    for (int iUnit = 0; iUnit < nUnits; ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        int nBricks = unit->GetBricksCount();

        for (int iBrick = 0; iBrick < nBricks; ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);

//            m_solver->Solve(brick);
        }
    }

    return false;
}
