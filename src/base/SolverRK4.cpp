#include "Processor.h"
#include "SolverRK4.h"

SolverRK4::SolverRK4()
    : Solver()
{
    m_nIterations = 4;
}

bool SolverRK4::Solve() {

    std::vector<Brick*>* bricks = m_processor->GetIterableBricksVectorPt();

    // Save the original state variables
    vecDoublePt* stateVariables = m_processor->GetStateVariablesVectorPt();
    int counter = 0;
    for (auto value: *stateVariables) {
        m_stateVariables(counter, 0) = *value;
        counter++;
    }

    // Compute the change rates for k1 = f(tn, Sn)
    int iRate = 0;
    for (auto brick: *bricks) {
        for (auto process: brick->GetProcesses()) {
            vecDouble rates = process->GetChangeRates();
            for (double rate: rates) {
                wxASSERT(m_changeRates.rows() > iRate);
                m_changeRates(iRate, 0) = rate;
                iRate++;
            }
        }
    }

    // Apply the changes
    iRate = 0;
    for (auto brick: *bricks) {
        brick->UpdateContentFromInputs();
        for (auto process: brick->GetProcesses()) {
            for (int iConnect = 0; iConnect < process->GetConnectionsNb(); ++iConnect) {
                process->ApplyChange(iConnect, m_changeRates(iRate, 0), *m_timeStepInDays);
                iRate++;
            }
        }
    }

    // Compute the change rates for k1 = f(tn + h, Sn + k1 h)
    iRate = 0;
    for (auto brick: *bricks) {
        for (auto process: brick->GetProcesses()) {
            vecDouble rates = process->GetChangeRates();
            for (double rate: rates) {
                wxASSERT(m_changeRates.rows() > iRate);
                m_changeRates(iRate, 1) = rate;
                iRate++;
            }
        }
    }

    // Restore original state variables
    counter = 0;
    for (auto value: *stateVariables) {
        *value = m_stateVariables(counter, 0);
        counter++;
    }
/*
    // Final change rates
    axd rkValues = (m_changeRates.col(0) + m_changeRates.col(1)) / 2;

    // Apply the changes
    iRate = 0;
    for (auto brick: *bricks) {
        brick->UpdateContentFromInputs();
        for (auto process: brick->GetProcesses()) {
            for (int iConnect = 0; iConnect < process->GetConnectionsNb(); ++iConnect) {
                process->ApplyChange(iConnect, rkValues(iRate), *m_timeStepInDays);
                iRate++;
            }
        }
    }*/

    // k1 = f(tn, Sn)
    // k2 = f(tn + h/2, Sn + k1 h / 2)
    // k3 = f(tn + h/2, Sn + k2 h / 2)
    // k4 = f(tn + h, Sn + k3 h)

    // Final change rate
    axd rkValues = (m_changeRates.col(0) + 2 * m_changeRates.col(1) +
                    2 * m_changeRates.col(2) + m_changeRates.col(3)) / 6;



    return true;
}