#include "Processor.h"
#include "SolverRK4.h"

SolverRK4::SolverRK4()
    : Solver()
{
    m_nIterations = 4;
}

bool SolverRK4::Solve() {

    std::vector<double*>* iterableValues = m_processor->GetIterableValuesVectorPt();
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

    // k2 = f(tn + h/2, Sn + k1 h / 2)
    for (auto brick: *iterableBricks) {
        brick->SetStateVariablesFor(0.5);
        if (!brick->Compute()) {
            return false;
        }
    }

    counter = 0;
    for (auto value: *iterableValues) {
        m_container(counter, 1) = *value;
        counter++;
    }

    // k3 = f(tn + h/2, Sn + k2 h / 2)
    for (auto brick: *iterableBricks) {
        brick->SetStateVariablesFor(0.5);
        if (!brick->Compute()) {
            return false;
        }
    }

    counter = 0;
    for (auto value: *iterableValues) {
        m_container(counter, 2) = *value;
        counter++;
    }

    // k4 = f(tn + h, Sn + k3 h)
    for (auto brick: *iterableBricks) {
        brick->SetStateVariablesFor(1.0);
        if (!brick->Compute()) {
            return false;
        }
    }

    counter = 0;
    for (auto value: *iterableValues) {
        m_container(counter, 3) = *value;
        counter++;
    }

    // Final change rate
    Eigen::ArrayXd rkValues = (m_container.col(0) + 2 * m_container.col(1) +
                               2 * m_container.col(2) + m_container.col(3)) / 6;

    // Update the state variable values
    for (int i = 0; i < (*iterableValues).size(); ++i) {
        *(*iterableValues)[i] = rkValues[i];
    }

    // Compute final values
    for (auto brick: *iterableBricks) {
        brick->SetStateVariablesFor(1.0);
        if (!brick->Compute()) {
            return false;
        }
    }

    return true;
}