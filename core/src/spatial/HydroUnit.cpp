#include "HydroUnit.h"

#include "SettingsBasin.h"

HydroUnit::HydroUnit(double area, Types type)
    : m_type(type),
      m_id(UNDEFINED),
      m_area(area) {}

HydroUnit::~HydroUnit() {
    for (auto forcing : m_forcing) {
        wxDELETE(forcing);
    }
    for (auto property : m_properties) {
        wxDELETE(property);
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

void HydroUnit::SetProperties(HydroUnitSettings& unitSettings) {
    m_id = unitSettings.id;

    for (const auto& unitProperty : unitSettings.propertiesDouble) {
        AddProperty(new HydroUnitProperty(unitProperty.name, unitProperty.value, unitProperty.unit));
    }

    for (const auto& unitProperty : unitSettings.propertiesString) {
        AddProperty(new HydroUnitProperty(unitProperty.name, unitProperty.value));
    }
}

void HydroUnit::AddProperty(HydroUnitProperty* property) {
    wxASSERT(property);
    m_properties.push_back(property);
}

double HydroUnit::GetPropertyDouble(const string& name, const string& unit) {
    for (auto property : m_properties) {
        if (property->GetName() == name) {
            return property->GetValue(unit);
        }
    }

    throw NotFound(wxString::Format(_("No property with the name '%s' was found."), name));
}

string HydroUnit::GetPropertyString(const string& name) {
    for (auto property : m_properties) {
        if (property->GetName() == name) {
            return property->GetValueString();
        }
    }

    throw NotFound(wxString::Format(_("No property with the name '%s' was found."), name));
}

void HydroUnit::AddBrick(Brick* brick) {
    wxASSERT(brick);
    m_bricks.push_back(brick);

    if (brick->IsLandCover()) {
        auto* landCover = dynamic_cast<LandCover*>(brick);
        m_landCoverBricks.push_back(landCover);
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

LandCover* HydroUnit::GetLandCover(const string& name) {
    for (auto brick : m_landCoverBricks) {
        if (brick->GetName() == name) {
            return brick;
        }
    }

    throw NotFound(wxString::Format(_("No land cover with the name '%s' was found."), name));
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
    if (!m_landCoverBricks.empty()) {
        double sumLandCoverArea = 0;
        for (auto brick : m_landCoverBricks) {
            sumLandCoverArea += brick->GetAreaFraction();
        }
        if (std::fabs(sumLandCoverArea - 1.0) > EPSILON_D) {
            wxLogError(_("The sum of the land cover fractions is not equal to 1, but equal to %f, with %f error margin."), sumLandCoverArea, EPSILON_D);
            return false;
        }
    }

    return true;
}

bool HydroUnit::ChangeLandCoverAreaFraction(const string& name, double fraction) {
    if ((fraction < 0) || (fraction > 1)) {
        wxLogError(_("The given fraction (%f) is not in the allowed range [0 .. 1]"), fraction);
        return false;
    }
    for (auto brick : m_landCoverBricks) {
        if (brick->GetName() == name) {
            brick->SetAreaFraction(fraction);
            return FixLandCoverFractionsTotal();
        }
    }
    wxLogError(_("Land cover '%s' was not found."), name);
    return false;
}

bool HydroUnit::FixLandCoverFractionsTotal() {
    LandCover* ground = nullptr;
    double total = 0;
    for (auto brick : m_landCoverBricks) {
        if (brick->GetName() == "ground" || brick->GetName() == "generic") {
            ground = brick;
        }
        total += brick->GetAreaFraction();
    }

    if (std::fabs(total - 1.0) > EPSILON_D) {
        double diff = total - 1.0;
        if (ground == nullptr) {
            wxLogError(_("No ground (generic) land cover found. Cannot fix the land cover fractions."));
            return false;
        }
        if (ground->GetAreaFraction() - diff < -EPSILON_D) {
            wxLogError(_("The ground (generic) land cover (%.20g) is not large enough to compensate "
                         "the area fractions (%.20g) with error margin (%.20g)."
                         "(i.e. the sum of the other land cover fractions is too large)."),
                       ground->GetAreaFraction(), diff, EPSILON_D);
            return false;
        }
        if (ground->GetAreaFraction() - diff > 0) {
            ground->SetAreaFraction(ground->GetAreaFraction() - diff);
        } else {
            ground->SetAreaFraction(0);
        }
    }

    return true;
}
