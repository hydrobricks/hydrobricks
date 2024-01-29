#include "ProcessOutflowOverflow.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowOverflow::ProcessOutflowOverflow(WaterContainer* container)
    : ProcessOutflow(container) {}

void ProcessOutflowOverflow::RegisterProcessParametersAndForcing(SettingsModel*) {
    // Nothing to register
}

void ProcessOutflowOverflow::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
}

vecDouble ProcessOutflowOverflow::GetRates() {
    return {0};
}

void ProcessOutflowOverflow::StoreInOutgoingFlux(double* rate, int index) {
    wxASSERT(m_outputs.size() > index);
    // Null the rate value as overflow will be handled at a later stage
    *rate = 0;
    m_outputs[index]->LinkChangeRate(rate);
}
