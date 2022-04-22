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

void Processor::ConnectToIterableElements() {
    SubBasin* basin = m_model->GetSubBasin();

    int nUnits = basin->GetHydroUnitsCount();

    for (int iUnit = 0; iUnit < nUnits; ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        int nBricks = unit->GetBricksCount();

        for (int iBrick = 0; iBrick < nBricks; ++iBrick) {
            // Get iterable elements from bricks
            Brick* brick = unit->GetBrick(iBrick);
            std::vector<double*> bricksElements = brick->GetIterableElements();
            StoreIterableElements(bricksElements);

            // Get iterable elements from processes
            std::vector<double*> processElements = brick->GetIterableElementsFromProcesses();
            StoreIterableElements(processElements);

            // Get iterable elements from fluxes
            std::vector<double*> fluxElements = brick->GetIterableElementsFromOutgoingFluxes();
            StoreIterableElements(fluxElements);
        }
    }
}

void Processor::StoreIterableElements(std::vector<double*>& elements) {
    if (!elements.empty()) {
        for (auto const& element : elements) {
            m_iterableElements.push_back(element);
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
