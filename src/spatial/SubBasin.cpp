#include "SubBasin.h"

SubBasin::SubBasin()
    : m_area(UNDEFINED),
      m_elevation(UNDEFINED),
      m_outletTotal(0)
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

double* SubBasin::GetValuePointer(const wxString& name) {
    if (name.IsSameAs("outlet")) {
        return &m_outletTotal;
    }
    wxLogError(_("Element '%s' not found"), name);

    return nullptr;
}

bool SubBasin::ComputeAggregatedValues() {
    m_outletTotal = 0;
    for (auto flux: m_outletFluxes) {
        m_outletTotal += flux->GetAmount();
    }

    return true;
}