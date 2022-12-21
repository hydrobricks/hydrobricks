#include "HydroUnit.h"

HydroUnit::HydroUnit(double area, Types type)
    : m_type(type),
      m_id(UNDEFINED),
      m_area(area) {}

HydroUnit::~HydroUnit() {
    for (auto forcing : m_forcing) {
        wxDELETE(forcing);
    }
}

void HydroUnit::Reset() {
    for (auto brick : m_bricks) {
        brick->Reset();
    }
}

void HydroUnit::SaveAsInitialState() {
    for (auto brick : m_bricks) {
        brick->SaveAsInitialState();
    }
}

void HydroUnit::AddProperty(HydroUnitProperty* property) {
    wxASSERT(property);
    m_properties.push_back(property);
}

void HydroUnit::AddBrick(Brick* brick) {
    wxASSERT(brick);
    m_bricks.push_back(brick);

    if (brick->IsLandCover()) {
        m_landCoverBricks.push_back(brick);
    }
}

void HydroUnit::AddSplitter(Splitter* splitter) {
    wxASSERT(splitter);
    m_splitters.push_back(splitter);
}

bool HydroUnit::HasForcing(VariableType type) {
    for (auto forcing : m_forcing) {
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
    for (auto forcing : m_forcing) {
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

bool HydroUnit::HasBrick(const string& name) {
    for (auto brick : m_bricks) {
        if (brick->GetName() == name) {
            return true;
        }
    }
    return false;
}

Brick* HydroUnit::GetBrick(const string& name) {
    for (auto brick : m_bricks) {
        if (brick->GetName() == name) {
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

bool HydroUnit::HasSplitter(const string& name) {
    for (auto splitter : m_splitters) {
        if (splitter->GetName() == name) {
            return true;
        }
    }
    return false;
}

Splitter* HydroUnit::GetSplitter(const string& name) {
    for (auto splitter : m_splitters) {
        if (splitter->GetName() == name) {
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
    if (m_area <= 0) {
        wxLogError(_("The hydro unit area has not been defined."));
        return false;
    }

    return true;
}
