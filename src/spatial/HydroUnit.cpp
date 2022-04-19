#include "HydroUnit.h"

HydroUnit::HydroUnit(float area, Types type)
    : m_type(type),
      m_id(UNDEFINED),
      m_area(area)
{}

HydroUnit::~HydroUnit() {
    for (auto& property : m_properties) {
        wxDELETE(property);
    }
    for (auto& container : m_containers) {
        wxDELETE(container);
    }
    for (auto& flux : m_fluxes) {
        wxDELETE(flux);
    }
}

void HydroUnit::AddProperty(HydroUnitProperty* property) {
    wxASSERT(property);
    m_properties.push_back(property);
}

void HydroUnit::AddContainer(Brick* container) {
    wxASSERT(container);
    m_containers.push_back(container);
}

void HydroUnit::AddFlux(Flux* flux) {
    wxASSERT(flux);
    m_fluxes.push_back(flux);
}