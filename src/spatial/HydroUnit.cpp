
#include "HydroUnit.h"

HydroUnit::HydroUnit(float area, Types type = Undefined)
    : m_area(area),
      m_type(type)
{}

void HydroUnit::AddProperty(HydroUnitProperty* property) {
    m_properties.push_back(property);
}