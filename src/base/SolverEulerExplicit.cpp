#include "Processor.h"
#include "SolverEulerExplicit.h"

SolverEulerExplicit::SolverEulerExplicit()
    : Solver()
{
    m_nIterations = 1;
}

bool SolverEulerExplicit::Solve() {

    std::vector<Brick*>* bricks = m_processor->GetIterableBricksVectorPt();

    // Compute the change rates
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
    
    return true;
}