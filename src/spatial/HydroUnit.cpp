#include "HydroUnit.h"

HydroUnit::HydroUnit(float area, Types type)
    : m_type(type),
      m_id(UNDEFINED),
      m_area(area),
      m_fluxInput(nullptr)
{}

HydroUnit::~HydroUnit() {
}

void HydroUnit::AddProperty(HydroUnitProperty* property) {
    wxASSERT(property);
    m_properties.push_back(property);
}

void HydroUnit::AddBrick(Brick* brick) {
    wxASSERT(brick);
    m_bricks.push_back(brick);
}

int HydroUnit::GetBricksCount() {
    return m_bricks.size();
}

Brick* GetBrick(int index) {
    wxASSERT(m_bricks.size() > index);
    wxASSERT(m_bricks[index]);

    return m_bricks[index];
}

void HydroUnit::SetInputFlux(Flux* flux) {
    wxASSERT(flux);
    m_fluxInput = flux;
}

bool HydroUnit::IsOk() {
    if (m_fluxInput == nullptr) {
        wxLogError(_("The unit input flux is not defined."));
        return false;
    }
    if (m_bricks.empty()) {
        wxLogError(_("The unit has no bricks attached."));
        return false;
    }
    for (auto brick : m_bricks) {
        if (!brick->IsOk()) return false;
    }
}