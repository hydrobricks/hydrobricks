
#include "SubBasin.h"

SubBasin::SubBasin() {}

void SubBasin::AddHydroUnit(HydroUnit* unit) {
    m_hydroUnits.push_back(unit);
}

int SubBasin::GetHydroUnitsCount() {
    return (int)m_hydroUnits.size();
}

bool SubBasin::HasIncomingFlow() {
    return !m_inConnectors.empty();
}

void SubBasin::AddInputConnector(Connector* connector) {
    wxASSERT(connector);
    m_inConnectors.push_back(connector);
}

void SubBasin::AddOutputConnector(Connector* connector) {
    wxASSERT(connector);
    m_outConnectors.push_back(connector);
}
