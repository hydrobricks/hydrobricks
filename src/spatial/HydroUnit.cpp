#include "HydroUnit.h"

HydroUnit::HydroUnit(float area, Types type)
    : m_type(type),
      m_id(UNDEFINED),
      m_area(area)
{}

HydroUnit::~HydroUnit() = default;

void HydroUnit::AddProperty(HydroUnitProperty* property) {
    wxASSERT(property);
    m_properties.push_back(property);
}

void HydroUnit::AddBrick(Brick* brick) {
    wxASSERT(brick);
    m_bricks.push_back(brick);
}

void HydroUnit::AddForcing(Forcing* forcing) {
    wxASSERT(forcing);
    m_forcing.push_back(forcing);
}

int HydroUnit::GetBricksCount() {
    return int(m_bricks.size());
}

Brick* HydroUnit::GetBrick(int index) {
    wxASSERT(m_bricks.size() > index);
    wxASSERT(m_bricks[index]);

    return m_bricks[index];
}

bool HydroUnit::IsOk() {
    if (m_bricks.empty()) {
        wxLogError(_("The unit has no bricks attached."));
        return false;
    }
    for (auto brick : m_bricks) {
        if (!brick->IsOk()) return false;
    }

    return true;
}