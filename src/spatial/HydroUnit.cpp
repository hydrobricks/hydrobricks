
#include "HydroUnit.h"

HydroUnit::HydroUnit(float area, Types type)
    : m_type(type),
      m_id(UNDEFINED),
      m_area(area)
{}

void HydroUnit::AddProperty(HydroUnitProperty* property) {
    wxASSERT(property);
    m_properties.push_back(property);
}

void HydroUnit::AddContainer(Container* container) {
    wxASSERT(container);
    m_containers.push_back(container);
}

void HydroUnit::AddFlux(Flux* flux) {
    wxASSERT(flux);
    m_fluxes.push_back(flux);
}