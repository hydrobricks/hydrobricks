#include "Processor.h"
#include "SolverEulerExplicit.h"

SolverEulerExplicit::SolverEulerExplicit()
    : Solver()
{
    m_nIterations = 1;
}

bool SolverEulerExplicit::Solve() {

    std::vector<Brick*>* iterableBricks = m_processor->GetIterableBricksVectorPt();

    for (auto brick: *iterableBricks) {
        brick->SetStateVariablesFor(0.0);
        if (!brick->Compute()) {
            return false;
        }
    }

    return true;
}