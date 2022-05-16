#include "HydroUnit.h"

HydroUnit::HydroUnit(float area, Types type)
    : m_type(type),
      m_id(UNDEFINED),
      m_area(area)
{}

HydroUnit::~HydroUnit() {
    for (auto forcing: m_forcing) {
        wxDELETE(forcing);
    }
}

void HydroUnit::AddProperty(HydroUnitProperty* property) {
    wxASSERT(property);
    m_properties.push_back(property);
}

void HydroUnit::AddBrick(Brick* brick) {
    wxASSERT(brick);
    m_bricks.push_back(brick);
}

void HydroUnit::AddSplitter(Splitter* splitter) {
    wxASSERT(splitter);
    m_splitters.push_back(splitter);
}

bool HydroUnit::HasForcing(VariableType type) {
    for (auto forcing: m_forcing) {
        if (forcing->GetType() == type) {
            return true;
        }
    }

    return false;
}

void HydroUnit::AddForcing(Forcing* forcing) {
    wxASSERT(forcing);
    m_forcing.push_back(forcing);
}

Forcing* HydroUnit::GetForcing(VariableType type) {
    for (auto forcing: m_forcing) {
        if (forcing->GetType() == type) {
            return forcing;
        }
    }

    return nullptr;
}

int HydroUnit::GetBricksCount() {
    return int(m_bricks.size());
}

int HydroUnit::GetSplittersCount() {
    return int(m_splitters.size());
}

Brick* HydroUnit::GetBrick(int index) {
    wxASSERT(m_bricks.size() > index);
    wxASSERT(m_bricks[index]);

    return m_bricks[index];
}

bool HydroUnit::HasBrick(const wxString &name) {
    for (auto brick: m_bricks) {
        if (brick->GetName().IsSameAs(name, false)) {
            return true;
        }
    }
    return false;
}

Brick* HydroUnit::GetBrick(const wxString &name) {
    for (auto brick: m_bricks) {
        if (brick->GetName().IsSameAs(name, false)) {
            return brick;
        }
    }

    throw NotFound(wxString::Format(_("No brick with the name '%s' was found."), name));
}

Splitter* HydroUnit::GetSplitter(int index) {
    wxASSERT(m_splitters.size() > index);
    wxASSERT(m_splitters[index]);

    return m_splitters[index];
}

bool HydroUnit::HasSplitter(const wxString &name) {
    for (auto splitter: m_splitters) {
        if (splitter->GetName().IsSameAs(name, false)) {
            return true;
        }
    }
    return false;
}

Splitter* HydroUnit::GetSplitter(const wxString &name) {
    for (auto splitter: m_splitters) {
        if (splitter->GetName().IsSameAs(name, false)) {
            return splitter;
        }
    }

    throw NotFound(wxString::Format(_("No splitter with the name '%s' was found."), name));
}

bool HydroUnit::IsOk() {
    for (auto brick : m_bricks) {
        if (!brick->IsOk()) return false;
    }
    for (auto splitter : m_splitters) {
        if (!splitter->IsOk()) return false;
    }

    return true;
}