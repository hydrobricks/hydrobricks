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

    // Restore original state variables
    counter = 0;
    for (auto value: *stateVariables) {
        *value = m_stateVariables(counter, 0);
        counter++;
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

    // Save the new state variables
    stateVariables = m_processor->GetStateVariablesVectorPt();
    counter = 0;
    for (auto value: *stateVariables) {
        m_stateVariables(counter, 1) = *value;
        counter++;
    }

    // Apply state variables for k1 at tn + h/2
    counter = 0;
    for (auto value: *stateVariables) {
        *value = (m_stateVariables(counter, 0) + m_stateVariables(counter, 1)) / 2;
        counter++;
    }

    // Compute the change rates for k2 = f(tn + h/2, Sn + k1 h/2)
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

    // Apply the changes
    iRate = 0;
    for (auto brick: *bricks) {
        brick->UpdateContentFromInputs();
        for (auto process: brick->GetProcesses()) {
            for (int iConnect = 0; iConnect < process->GetConnectionsNb(); ++iConnect) {
                process->ApplyChange(iConnect, m_changeRates(iRate, 1), *m_timeStepInDays);
                iRate++;
            }
        }
    }

    // Save the new state variables
    stateVariables = m_processor->GetStateVariablesVectorPt();
    counter = 0;
    for (auto value: *stateVariables) {
        m_stateVariables(counter, 2) = *value;
        counter++;
    }

    // Apply state variables for k2 at tn + h/2
    counter = 0;
    for (auto value: *stateVariables) {
        *value = (m_stateVariables(counter, 0) + m_stateVariables(counter, 2)) / 2;
        counter++;
    }

    // Compute the change rates for k3 = f(tn + h/2, Sn + k2 h/2)
    iRate = 0;
    for (auto brick: *bricks) {
        for (auto process: brick->GetProcesses()) {
            vecDouble rates = process->GetChangeRates();
            for (double rate: rates) {
                wxASSERT(m_changeRates.rows() > iRate);
                m_changeRates(iRate, 2) = rate;
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

    // Apply the changes
    iRate = 0;
    for (auto brick: *bricks) {
        brick->UpdateContentFromInputs();
        for (auto process: brick->GetProcesses()) {
            for (int iConnect = 0; iConnect < process->GetConnectionsNb(); ++iConnect) {
                process->ApplyChange(iConnect, m_changeRates(iRate, 2), *m_timeStepInDays);
                iRate++;
            }
        }
    }

    // Save the new state variables
    stateVariables = m_processor->GetStateVariablesVectorPt();
    counter = 0;
    for (auto value: *stateVariables) {
        m_stateVariables(counter, 3) = *value;
        counter++;
    }

    // Apply state variables for k3 at tn + h
    counter = 0;
    for (auto value: *stateVariables) {
        *value = m_stateVariables(counter, 3);
        counter++;
    }

    // Compute the change rates for k4 = f(tn + h, Sn + k3 h)
    iRate = 0;
    for (auto brick: *bricks) {
        for (auto process: brick->GetProcesses()) {
            vecDouble rates = process->GetChangeRates();
            for (double rate: rates) {
                wxASSERT(m_changeRates.rows() > iRate);
                m_changeRates(iRate, 3) = rate;
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

    // Final change rate
    axd rkValues = (m_changeRates.col(0) + 2 * m_changeRates.col(1) +
                    2 * m_changeRates.col(2) + m_changeRates.col(3)) / 6;

    // Apply the final rates
    iRate = 0;
    for (auto brick: *bricks) {
        brick->UpdateContentFromInputs();
        for (auto process: brick->GetProcesses()) {
            for (int iConnect = 0; iConnect < process->GetConnectionsNb(); ++iConnect) {
                process->ApplyChange(iConnect, rkValues(iRate), *m_timeStepInDays);
                iRate++;
            }
        }
    }

    return true;
}