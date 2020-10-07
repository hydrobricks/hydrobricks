
#include "SubCatchment.h"

SubCatchment::SubCatchment() {}

void SubCatchment::AddHydroUnit(HydroUnit* unit) {
    m_hydroUnits.push_back(unit);
}

bool SubCatchment::HasIncomingFlow() {
    return !m_inConnectors.empty();
}

void SubCatchment::AddInputConnector(Connector* connector) {
    wxASSERT(connector);
    m_inConnectors.push_back(connector);
}

void SubCatchment::AddOutputConnector(Connector* connector) {
    wxASSERT(connector);
    m_outConnectors.push_back(connector);
}