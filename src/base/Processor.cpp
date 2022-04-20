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

bool Processor::ProcessTimeStep() {
    wxASSERT(m_model);

    SubBasin* basin = m_model->GetSubBasin();
    int nUnits = basin->GetHydroUnitsCount();

    for (int iUnit = 0; iUnit < nUnits; ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        int nBricks = unit->GetBricksCount();

        for (int iBrick = 0; iBrick < nBricks; ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);

            m_solver->Solve(brick);
        }
    }

    return false;
}
