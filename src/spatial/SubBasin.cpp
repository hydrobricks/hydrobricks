
#include "SubBasin.h"

SubBasin::SubBasin()
    : m_area(UNDEFINED)
{}

SubBasin::~SubBasin() {
    for (auto& hydroUnit : m_hydroUnits) {
        wxDELETE(hydroUnit);
    }
    for (auto& outConnector : m_outConnectors) {
        // Only delete "out" connectors, "in" connectors will be deleted by the source sub basin.
        wxDELETE(outConnector);
    }
    for (auto& lumpedContainer : m_lumpedContainers) {
        wxDELETE(lumpedContainer);
    }
    for (auto& lumpedFlux : m_lumpedFluxes) {
        wxDELETE(lumpedFlux);
    }
    for (auto& behaviour : m_behaviours) {
        wxDELETE(behaviour);
    }
}

bool SubBasin::IsOk() {
    if (m_hydroUnits.empty()) return false;

    return true;
}

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

void SubBasin::AddContainer(Container* container) {
    wxASSERT(container);
    m_lumpedContainers.push_back(container);
}

void SubBasin::AddBehaviour(Behaviour* behaviour) {
    wxASSERT(behaviour);
    m_behaviours.push_back(behaviour);
}

void SubBasin::AddFlux(Flux* flux) {
    wxASSERT(flux);
    m_lumpedFluxes.push_back(flux);
}