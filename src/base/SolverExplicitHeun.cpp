#include "Processor.h"
#include "SolverExplicitHeun.h"

SolverExplicitHeun::SolverExplicitHeun()
    : Solver()
{
    m_nIterations = 2;
}

bool SolverExplicitHeun::Solve() {

    vecDoublePt* iterableValues = m_processor->GetIterableValuesVectorPt();
    std::vector<Brick*>* iterableBricks = m_processor->GetIterableBricksVectorPt();

    // k1 = f(tn, Sn)
    for (auto brick: *iterableBricks) {
        brick->SetStateVariablesFor(0.0);
        if (!brick->Compute()) {
            return false;
        }
    }

    int counter = 0;
    for (auto value: *iterableValues) {
        m_container(counter, 0) = *value;
        counter++;
    }

    // k2 = f(tn + h, Sn + k1 h)
    for (auto brick: *iterableBricks) {
        brick->SetStateVariablesFor(1.0);
        if (!brick->Compute()) {
            return false;
        }
    }

    counter = 0;
    for (auto value: *iterableValues) {
        m_container(counter, 1) = *value;
        counter++;
    }

    // Final change rate
    axd rkValues = (m_container.col(0) + m_container.col(1)) / 2;

    // Update the state variable values
    for (int i = 0; i < (*iterableValues).size(); ++i) {
        *(*iterableValues)[i] = rkValues[i];
    }

    // Compute final values
    for (auto brick: *iterableBricks) {
        brick->SetStateVariablesFor(0.0);
        if (!brick->Compute()) {
            return false;
        }
    }

    return true;
}