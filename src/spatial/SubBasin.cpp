#include "SubBasin.h"

SubBasin::SubBasin()
    : m_area(UNDEFINED),
      m_elevation(UNDEFINED)
{}

SubBasin::~SubBasin() {}

bool SubBasin::IsOk() {
    if (m_hydroUnits.empty()) {
        wxLogError(_("The sub basin has no hydro unit attached."));
        return false;
    }
    for (auto unit : m_hydroUnits) {
        if (!unit->IsOk()) return false;
    }

    return true;
}

void SubBasin::AddHydroUnit(HydroUnit* unit) {
    m_hydroUnits.push_back(unit);
}

int SubBasin::GetHydroUnitsNb() {
    return int(m_hydroUnits.size());
}

HydroUnit* SubBasin::GetHydroUnit(int index) {
    wxASSERT(m_hydroUnits.size() > index);
    wxASSERT(m_hydroUnits[index]);

    return m_hydroUnits[index];
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

void SubBasin::AddBehaviour(Behaviour* behaviour) {
    wxASSERT(behaviour);
    m_behaviours.push_back(behaviour);
}

void SubBasin::AttachOutletFlux(Flux* flux) {
    wxASSERT(flux);
    m_outletFluxes.push_back(flux);
}
