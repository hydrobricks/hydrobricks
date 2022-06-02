#include "Parameter.h"
#include "SettingsBasin.h"

SettingsBasin::SettingsBasin()
{
}

SettingsBasin::~SettingsBasin() {
}

void SettingsBasin::AddHydroUnit(int id, double area) {
    HydroUnitSettings unit;
    unit.id = id;
    unit.area = area;
    m_hydroUnits.push_back(unit);
    m_selectedHydroUnit = &m_hydroUnits[m_hydroUnits.size() - 1];
}

void SettingsBasin::AddSurfaceElementToCurrentUnit(const wxString& name, double fraction) {
    wxASSERT(m_selectedHydroUnit);
    SurfaceElementSettings element;
    element.name = name;
    element.fraction = fraction;
    m_selectedHydroUnit->surfaceElements.push_back(element);
}

void SettingsBasin::SelectUnit(int index) {
    wxASSERT(m_hydroUnits.size() > index);
    m_selectedHydroUnit = &m_hydroUnits[index];
}

