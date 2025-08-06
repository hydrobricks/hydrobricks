#include "HydroUnit.h"

#include "SettingsBasin.h"

HydroUnit::HydroUnit(double area, Types type)
    : _type(type),
      _id(UNDEFINED),
      _area(area) {}

HydroUnit::~HydroUnit() {
    for (auto forcing : _forcing) {
        wxDELETE(forcing);
    }
    for (auto property : _properties) {
        wxDELETE(property);
    }
}

void HydroUnit::Reset() {
    for (auto brick : _bricks) {
        brick->Reset();
    }
}

void HydroUnit::SaveAsInitialState() {
    for (auto brick : _bricks) {
        brick->SaveAsInitialState();
    }
}

void HydroUnit::SetProperties(HydroUnitSettings& unitSettings) {
    _id = unitSettings.id;

    for (const auto& unitProperty : unitSettings.propertiesDouble) {
        AddProperty(new HydroUnitProperty(unitProperty.name, unitProperty.value, unitProperty.unit));
    }

    for (const auto& unitProperty : unitSettings.propertiesString) {
        AddProperty(new HydroUnitProperty(unitProperty.name, unitProperty.value));
    }
}

void HydroUnit::AddProperty(HydroUnitProperty* property) {
    wxASSERT(property);
    _properties.push_back(property);
}

double HydroUnit::GetPropertyDouble(const string& name, const string& unit) {
    for (auto property : _properties) {
        if (property->GetName() == name) {
            return property->GetValue(unit);
        }
    }

    throw NotFound(wxString::Format(_("No property with the name '%s' was found."), name));
}

string HydroUnit::GetPropertyString(const string& name) {
    for (auto property : _properties) {
        if (property->GetName() == name) {
            return property->GetValueString();
        }
    }

    throw NotFound(wxString::Format(_("No property with the name '%s' was found."), name));
}

void HydroUnit::AddBrick(Brick* brick) {
    wxASSERT(brick);
    _bricks.push_back(brick);

    brick->SetHydroUnit(this);

    if (brick->IsLandCover()) {
        auto* landCover = dynamic_cast<LandCover*>(brick);
        _landCoverBricks.push_back(landCover);
    }
}

void HydroUnit::AddSplitter(Splitter* splitter) {
    wxASSERT(splitter);
    _splitters.push_back(splitter);
}

bool HydroUnit::HasForcing(VariableType type) {
    for (auto forcing : _forcing) {
        if (forcing->GetType() == type) {
            return true;
        }
    }

    return false;
}

void HydroUnit::AddForcing(Forcing* forcing) {
    wxASSERT(forcing);
    _forcing.push_back(forcing);
}

Forcing* HydroUnit::GetForcing(VariableType type) {
    for (auto forcing : _forcing) {
        if (forcing->GetType() == type) {
            return forcing;
        }
    }

    return nullptr;
}

void HydroUnit::AddLateralConnection(HydroUnit* receiver, double fraction, const string& type) {
    wxASSERT(receiver);
    if (fraction <= 0 || fraction > 1) {
        throw ConceptionIssue(wxString::Format(_("The fraction (%f) is not in the range ]0 .. 1]"), fraction));
    }

    _lateralConnections.push_back(new HydroUnitLateralConnection(receiver, fraction, type));
}

int HydroUnit::GetBricksCount() {
    return static_cast<int>(_bricks.size());
}

int HydroUnit::GetSplittersCount() {
    return static_cast<int>(_splitters.size());
}

Brick* HydroUnit::GetBrick(int index) {
    wxASSERT(_bricks.size() > index);
    wxASSERT(_bricks[index]);

    return _bricks[index];
}

bool HydroUnit::HasBrick(const string& name) {
    for (auto brick : _bricks) {
        if (brick->GetName() == name) {
            return true;
        }
    }
    return false;
}

Brick* HydroUnit::GetBrick(const string& name) {
    for (auto brick : _bricks) {
        if (brick->GetName() == name) {
            return brick;
        }
    }

    throw NotFound(wxString::Format(_("No brick with the name '%s' was found."), name));
}

LandCover* HydroUnit::GetLandCover(const string& name) {
    for (auto brick : _landCoverBricks) {
        if (brick->GetName() == name) {
            return brick;
        }
    }

    throw NotFound(wxString::Format(_("No land cover with the name '%s' was found."), name));
}

Splitter* HydroUnit::GetSplitter(int index) {
    wxASSERT(_splitters.size() > index);
    wxASSERT(_splitters[index]);

    return _splitters[index];
}

bool HydroUnit::HasSplitter(const string& name) {
    for (auto splitter : _splitters) {
        if (splitter->GetName() == name) {
            return true;
        }
    }
    return false;
}

Splitter* HydroUnit::GetSplitter(const string& name) {
    for (auto splitter : _splitters) {
        if (splitter->GetName() == name) {
            return splitter;
        }
    }

    throw NotFound(wxString::Format(_("No splitter with the name '%s' was found."), name));
}

bool HydroUnit::IsOk() {
    for (auto brick : _bricks) {
        if (!brick->IsOk()) return false;
    }
    for (auto splitter : _splitters) {
        if (!splitter->IsOk()) return false;
    }
    if (_area <= 0) {
        wxLogError(_("The hydro unit area has not been defined."));
        return false;
    }
    if (!_landCoverBricks.empty()) {
        double sumLandCoverArea = 0;
        for (auto brick : _landCoverBricks) {
            sumLandCoverArea += brick->GetAreaFraction();
        }
        if (std::fabs(sumLandCoverArea - 1.0) > EPSILON_D) {
            if (std::fabs(sumLandCoverArea - 1.0) > 0.0001) {
                wxLogError(_("The sum of the land cover fractions is not equal to 1, "
                             "but equal to %f, with %f error margin."),
                           sumLandCoverArea, 0.0001);
                return false;
            }

            // If the error is small, we can fix it (e.g., due to rounding errors).
            double diff = sumLandCoverArea - 1.0;

            // Assign the difference to the first land cover brick with a positive area fraction.
            for (auto brick : _landCoverBricks) {
                if (brick->GetAreaFraction() > diff) {
                    brick->SetAreaFraction(brick->GetAreaFraction() - diff);
                    break;
                }
            }
        }
    }

    return true;
}

bool HydroUnit::ChangeLandCoverAreaFraction(const string& name, double fraction) {
    if ((fraction < 0) || (fraction > 1)) {
        wxLogError(_("The given fraction (%f) is not in the allowed range [0 .. 1]"), fraction);
        return false;
    }
    for (auto brick : _landCoverBricks) {
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
    for (auto brick : _landCoverBricks) {
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
