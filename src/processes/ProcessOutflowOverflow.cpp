#include "Brick.h"
#include "ProcessOutflowOverflow.h"

ProcessOutflowOverflow::ProcessOutflowOverflow(Brick* brick)
    : ProcessOutflow(brick)
{}

void ProcessOutflowOverflow::AssignParameters(const ProcessSettings &processSettings) {
    Process::AssignParameters(processSettings);
}

vecDouble ProcessOutflowOverflow::GetChangeRates() {
    return {0};
}

void ProcessOutflowOverflow::StoreInOutgoingFlux(double* rate, int index) {
    wxASSERT(m_outputs.size() > index);
    // Null the rate value as overflow will be handled at a later stage
    *rate = 0;
    m_outputs[index]->LinkChangeRate(rate);
}