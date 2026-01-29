#include "HydroUnit.h"

#include <utility>

#include "SettingsBasin.h"

HydroUnit::HydroUnit(double area, Types type)
    : _type(type),
      _id(UNDEFINED),
      _area(area) {}

HydroUnit::~HydroUnit() {
    // Unique_ptr members clean up owned objects automatically.
}

void HydroUnit::ReserveBricks(size_t count) {
    _bricks.reserve(_bricks.size() + count);
}

void HydroUnit::ReserveLandCoverBricks(size_t count) {
    _landCoverBricks.reserve(_landCoverBricks.size() + count);
}

void HydroUnit::ReserveSplitters(size_t count) {
    _splitters.reserve(_splitters.size() + count);
}

void HydroUnit::ReserveLateralConnections(size_t count) {
    _lateralConnections.reserve(_lateralConnections.size() + count);
}

void HydroUnit::ReserveForcings(size_t count) {
    _forcing.reserve(_forcing.size() + count);
}

void HydroUnit::Reset() {
    for (const auto& brick : _bricks) {
        brick->Reset();
    }
}

void HydroUnit::SaveAsInitialState() {
    for (const auto& brick : _bricks) {
        brick->SaveAsInitialState();
    }
}

void HydroUnit::SetProperties(HydroUnitSettings& unitSettings) {
    _id = unitSettings.id;

    // Reserve once to avoid reallocations when adding known properties.
    size_t propertiesCount = unitSettings.propertiesDouble.size() + unitSettings.propertiesString.size();
    _properties.reserve(_properties.size() + propertiesCount);

    for (const auto& unitProperty : unitSettings.propertiesDouble) {
        AddProperty(std::make_unique<HydroUnitProperty>(unitProperty.name, unitProperty.value, unitProperty.unit));
    }

    for (const auto& unitProperty : unitSettings.propertiesString) {
        AddProperty(std::make_unique<HydroUnitProperty>(unitProperty.name, unitProperty.value));
    }
}

void HydroUnit::AddProperty(std::unique_ptr<HydroUnitProperty> property) {
    wxASSERT(property);
    _properties.push_back(std::move(property));
}

double HydroUnit::GetPropertyDouble(const string& name, const string& unit) const {
    for (const auto& property : _properties) {
        if (property->GetName() == name) {
            return property->GetValue(unit);
        }
    }

    throw ModelConfigError(wxString::Format(_("No property with the name '%s' was found."), name));
}

string HydroUnit::GetPropertyString(const string& name) const {
    for (const auto& property : _properties) {
        if (property->GetName() == name) {
            return property->GetValueString();
        }
    }

    throw ModelConfigError(wxString::Format(_("No property with the name '%s' was found."), name));
}

void HydroUnit::AddBrick(std::unique_ptr<Brick> brick) {
    wxASSERT(brick);
    Brick* rawBrick = brick.get();
    _brickMap[rawBrick->GetName()] = rawBrick;
    if (rawBrick->IsLandCover()) {
        auto* landCover = dynamic_cast<LandCover*>(rawBrick);
        _landCoverBricks.push_back(landCover);
        _landCoverMap[landCover->GetName()] = landCover;
    }
    rawBrick->SetHydroUnit(this);
    _bricks.push_back(std::move(brick));
}

void HydroUnit::AddSplitter(std::unique_ptr<Splitter> splitter) {
    wxASSERT(splitter);
    Splitter* rawSplitter = splitter.get();
    _splitterMap[rawSplitter->GetName()] = rawSplitter;
    _splitters.push_back(std::move(splitter));
}

bool HydroUnit::HasForcing(VariableType type) {
    return _forcingMap.find(type) != _forcingMap.end();
}

void HydroUnit::AddForcing(std::unique_ptr<Forcing> forcing) {
    wxASSERT(forcing);
    _forcingMap[forcing->GetType()] = forcing.get();
    _forcing.push_back(std::move(forcing));
}

Forcing* HydroUnit::GetForcing(VariableType type) const {
    auto it = _forcingMap.find(type);
    if (it != _forcingMap.end()) {
        return it->second;
    }

    return nullptr;
}

void HydroUnit::AddLateralConnection(HydroUnit* receiver, double fraction, const string& type) {
    wxASSERT(receiver);
    if (fraction <= 0 || fraction > 1) {
        throw ModelConfigError(wxString::Format(_("The fraction (%f) is not in the range ]0 .. 1]"), fraction));
    }
    _lateralConnections.push_back(std::make_unique<HydroUnitLateralConnection>(receiver, fraction, type));
}

int HydroUnit::GetBricksCount() const {
    return static_cast<int>(_bricks.size());
}

int HydroUnit::GetSplittersCount() const {
    return static_cast<int>(_splitters.size());
}

Brick* HydroUnit::GetBrick(size_t index) const {
    wxASSERT(_bricks.size() > index);
    wxASSERT(_bricks[index]);

    return _bricks[index].get();
}

bool HydroUnit::HasBrick(const string& name) const {
    return _brickMap.find(name) != _brickMap.end();
}

Brick* HydroUnit::GetBrick(const string& name) const {
    auto it = _brickMap.find(name);
    if (it != _brickMap.end()) {
        return it->second;
    }

    throw ModelConfigError(wxString::Format(_("No brick with the name '%s' was found."), name));
}

Brick* HydroUnit::TryGetBrick(const string& name) const {
    auto it = _brickMap.find(name);
    return it != _brickMap.end() ? it->second : nullptr;
}

LandCover* HydroUnit::GetLandCover(const string& name) const {
    auto it = _landCoverMap.find(name);
    if (it != _landCoverMap.end()) {
        return it->second;
    }

    throw ModelConfigError(wxString::Format(_("No land cover with the name '%s' was found."), name));
}

LandCover* HydroUnit::TryGetLandCover(const string& name) const {
    auto it = _landCoverMap.find(name);
    return it != _landCoverMap.end() ? it->second : nullptr;
}

Splitter* HydroUnit::GetSplitter(size_t index) const {
    wxASSERT(_splitters.size() > index);
    wxASSERT(_splitters[index]);

    return _splitters[index].get();
}

bool HydroUnit::HasSplitter(const string& name) const {
    return _splitterMap.find(name) != _splitterMap.end();
}

Splitter* HydroUnit::GetSplitter(const string& name) const {
    auto it = _splitterMap.find(name);
    if (it != _splitterMap.end()) {
        return it->second;
    }

    throw ModelConfigError(wxString::Format(_("No splitter with the name '%s' was found."), name));
}

Splitter* HydroUnit::TryGetSplitter(const string& name) const {
    auto it = _splitterMap.find(name);
    return it != _splitterMap.end() ? it->second : nullptr;
}

bool HydroUnit::IsValid(bool checkProcesses) const {
    for (const auto& brick : _bricks) {
        if (!brick->IsValid(checkProcesses)) return false;
    }
    for (const auto& splitter : _splitters) {
        if (!splitter->IsValid()) return false;
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
        if (!NearlyEqual(sumLandCoverArea, 1.0, EPSILON_D)) {
            if (!NearlyEqual(sumLandCoverArea, 1.0, TOLERANCE_LOOSE)) {
                wxLogError(_("The sum of the land cover fractions is not equal to 1, "
                             "but equal to %f, with %f error margin."),
                           sumLandCoverArea, TOLERANCE_LOOSE);
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

void HydroUnit::Validate() const {
    if (!IsValid()) {
        throw ModelConfigError(
            _("HydroUnit validation failed. Check area, bricks, splitters, and land cover fractions."));
    }
}

bool HydroUnit::ChangeLandCoverAreaFraction(const string& name, double fraction) {
    if ((fraction < 0) || (fraction > 1)) {
        wxLogError(_("The given fraction (%f) for '%s' is not in the allowed range [0 .. 1]"), fraction, name);
        return false;
    }
    auto it = _landCoverMap.find(name);
    if (it != _landCoverMap.end()) {
        it->second->SetAreaFraction(fraction);
        return FixLandCoverFractionsTotal();
    }
    wxLogError(_("Land cover '%s' was not found."), name);
    return false;
}

bool HydroUnit::FixLandCoverFractionsTotal() {
    // Try to find ground or generic land cover using the map
    LandCover* ground = nullptr;
    auto groundIt = _landCoverMap.find("ground");
    if (groundIt != _landCoverMap.end()) {
        ground = groundIt->second;
    } else {
        auto genericIt = _landCoverMap.find("generic");
        if (genericIt != _landCoverMap.end()) {
            ground = genericIt->second;
        }
    }

    double total = 0;
    for (auto brick : _landCoverBricks) {
        total += brick->GetAreaFraction();
    }

    if (!NearlyEqual(total, 1.0, EPSILON_D)) {
        double diff = total - 1.0;
        if (ground == nullptr) {
            wxLogError(_("No ground (generic) land cover found. Cannot fix the land cover fractions."));
            return false;
        }
        if (LessThan(ground->GetAreaFraction(), diff, EPSILON_D)) {
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

std::vector<HydroUnitLateralConnection*> HydroUnit::GetLateralConnections() const {
    std::vector<HydroUnitLateralConnection*> views;
    views.reserve(_lateralConnections.size());
    for (const auto& conn : _lateralConnections) {
        views.push_back(conn.get());
    }
    return views;
}
