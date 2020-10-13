
#include "HydroUnit.h"

HydroUnit::HydroUnit(float area, Types type)
    : m_type(type),
      m_area(area)
{}

void HydroUnit::AddProperty(HydroUnitProperty* property) {
    m_properties.push_back(property);
}

void HydroUnit::AddContainer(Container* container) {
    m_containers.push_back(container);
}