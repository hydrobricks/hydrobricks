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
    return false;
}
