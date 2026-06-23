#include "HydroUnit.h"

#include <cmath>
#include <utility>

#include "SettingsBasin.h"

namespace {

// Identifies a single content storage exposed by a land cover, used to pair the
// matching storages of two covers when water is transferred between them.
enum class CoverRole {
    SelfWater,      // the land cover's own water container
    SelfIce,        // the land cover's ice container (glaciers only)
    Snow,           // the snow held by the cover's snowpack
    SnowpackWater,  // the liquid water retained in the cover's snowpack
    CanopyWater     // the water intercepted by the cover's canopy
};

struct ContentSlot {
    Brick* brick;
    ContentType type;
    CoverRole role;
};

// Build the list of content storages a land cover exposes: its own water (and ice
// for glaciers) plus the water/snow held by its snowpack and canopy surface
// components (named '<cover>_snowpack' / '<cover>_canopy'). Infinite storages (e.g.
// an unlimited glacier ice reservoir) are not tracked masses and cannot be
// reassigned, so they are excluded from the transfer.
vector<ContentSlot> GetContentSlots(LandCover* cover, const HydroUnit* unit) {
    vector<ContentSlot> slots;
    auto addSlot = [&slots](Brick* brick, ContentType type, CoverRole role) {
        if (!std::isinf(brick->GetContent(type))) {
            slots.push_back({brick, type, role});
        }
    };
    addSlot(cover, ContentType::Water, CoverRole::SelfWater);
    if (cover->GetCategory() == BrickCategory::Glacier) {
        addSlot(cover, ContentType::Ice, CoverRole::SelfIce);
    }
    Brick* snowpack = unit->TryGetBrick(string(cover->GetName()) + "_snowpack");
    if (snowpack != nullptr) {
        addSlot(snowpack, ContentType::Snow, CoverRole::Snow);
        addSlot(snowpack, ContentType::Water, CoverRole::SnowpackWater);
    }
    Brick* canopy = unit->TryGetBrick(string(cover->GetName()) + "_canopy");
    if (canopy != nullptr) {
        addSlot(canopy, ContentType::Water, CoverRole::CanopyWater);
    }
    return slots;
}

double GetSlotDepth(const vector<ContentSlot>& slots, CoverRole role) {
    for (const auto& slot : slots) {
        if (slot.role == role) {
            return slot.brick->GetContent(slot.type);
        }
    }
    return 0;
}

bool HasRole(const vector<ContentSlot>& slots, CoverRole role) {
    for (const auto& slot : slots) {
        if (slot.role == role) {
            return true;
        }
    }
    return false;
}

}  // namespace

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
    size_t propertyCount = unitSettings.propertiesDouble.size() + unitSettings.propertiesString.size();
    _properties.reserve(_properties.size() + propertyCount);

    for (const auto& unitProperty : unitSettings.propertiesDouble) {
        AddProperty(std::make_unique<HydroUnitProperty>(unitProperty.name, unitProperty.value, unitProperty.unit));
    }

    for (const auto& unitProperty : unitSettings.propertiesString) {
        AddProperty(std::make_unique<HydroUnitProperty>(unitProperty.name, unitProperty.value));
    }
}

void HydroUnit::AddProperty(std::unique_ptr<HydroUnitProperty> property) {
    assert(property);
    _properties.push_back(std::move(property));
}

double HydroUnit::GetPropertyDouble(std::string_view name, std::string_view unit) const {
    for (const auto& property : _properties) {
        if (property->GetName() == name) {
            return property->GetValue(unit);
        }
    }

    throw ModelConfigError(std::format("No property with the name '{}' was found.", name));
}

float HydroUnit::GetPropertyFloat(std::string_view name, std::string_view unit) const {
    return static_cast<float>(GetPropertyDouble(name, unit));
}

string HydroUnit::GetPropertyString(std::string_view name) const {
    for (const auto& property : _properties) {
        if (property->GetName() == name) {
            return property->GetValueString();
        }
    }

    throw ModelConfigError(std::format("No property with the name '{}' was found.", name));
}

void HydroUnit::AddBrick(std::unique_ptr<Brick> brick) {
    assert(brick);
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
    assert(splitter);
    Splitter* rawSplitter = splitter.get();
    _splitterMap[rawSplitter->GetName()] = rawSplitter;
    _splitters.push_back(std::move(splitter));
}

bool HydroUnit::HasForcing(VariableType type) const {
    return _forcingMap.find(type) != _forcingMap.end();
}

void HydroUnit::AddForcing(std::unique_ptr<Forcing> forcing) {
    assert(forcing);
    _forcingMap[forcing->GetType()] = forcing.get();
    _forcing.push_back(std::move(forcing));
}

void HydroUnit::ResetForcingUpdates() {
    for (const auto& forcing : _forcing) {
        forcing->ResetUpdate();
    }
}

Forcing* HydroUnit::GetForcing(VariableType type) const {
    auto it = _forcingMap.find(type);
    if (it != _forcingMap.end()) {
        return it->second;
    }

    return nullptr;
}

void HydroUnit::AddLateralConnection(HydroUnit* receiver, double fraction, const string& type) {
    assert(receiver);
    if (fraction <= 0 || fraction > 1) {
        throw ModelConfigError(std::format("The fraction ({}) is not in the range ]0 .. 1]", fraction));
    }
    _lateralConnections.push_back(std::make_unique<HydroUnitLateralConnection>(receiver, fraction, type));
}

int HydroUnit::GetBrickCount() const {
    return static_cast<int>(_bricks.size());
}

int HydroUnit::GetSplitterCount() const {
    return static_cast<int>(_splitters.size());
}

Brick* HydroUnit::GetBrick(size_t index) const {
    assert(_bricks.size() > index);
    assert(_bricks[index]);

    return _bricks[index].get();
}

bool HydroUnit::HasBrick(std::string_view name) const {
    return _brickMap.find(string(name)) != _brickMap.end();
}

Brick* HydroUnit::GetBrick(std::string_view name) const {
    auto it = _brickMap.find(string(name));
    if (it != _brickMap.end()) {
        return it->second;
    }

    throw ModelConfigError(std::format("No brick with the name '{}' was found.", name));
}

Brick* HydroUnit::TryGetBrick(std::string_view name) const {
    auto it = _brickMap.find(string(name));
    return it != _brickMap.end() ? it->second : nullptr;
}

LandCover* HydroUnit::GetLandCover(std::string_view name) const {
    auto it = _landCoverMap.find(string(name));
    if (it != _landCoverMap.end()) {
        return it->second;
    }

    throw ModelConfigError(std::format("No land cover with the name '{}' was found.", name));
}

LandCover* HydroUnit::TryGetLandCover(std::string_view name) const {
    auto it = _landCoverMap.find(string(name));
    return it != _landCoverMap.end() ? it->second : nullptr;
}

Splitter* HydroUnit::GetSplitter(size_t index) const {
    assert(_splitters.size() > index);
    assert(_splitters[index]);

    return _splitters[index].get();
}

bool HydroUnit::HasSplitter(std::string_view name) const {
    return _splitterMap.find(string(name)) != _splitterMap.end();
}

Splitter* HydroUnit::GetSplitter(std::string_view name) const {
    auto it = _splitterMap.find(string(name));
    if (it != _splitterMap.end()) {
        return it->second;
    }

    throw ModelConfigError(std::format("No splitter with the name '{}' was found.", name));
}

Splitter* HydroUnit::TryGetSplitter(std::string_view name) const {
    auto it = _splitterMap.find(string(name));
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
        LogError("The hydro unit area has not been defined.");
        return false;
    }
    if (!_landCoverBricks.empty()) {
        double sumLandCoverArea = 0;
        for (auto brick : _landCoverBricks) {
            sumLandCoverArea += brick->GetAreaFraction();
        }
        if (!NearlyEqual(sumLandCoverArea, 1.0, EPSILON_D)) {
            if (!NearlyEqual(sumLandCoverArea, 1.0, TOLERANCE_LOOSE)) {
                LogError(
                    "The sum of the land cover fractions is not equal to 1, "
                    "but equal to %f, with %f error margin.",
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
        throw ModelConfigError("HydroUnit validation failed. Check area, bricks, splitters, and land cover fractions.");
    }
}

bool HydroUnit::ChangeLandCoverAreaFraction(std::string_view name, double fraction) {
    if ((fraction < 0) || (fraction > 1)) {
        LogError("The given fraction ({}) for '{}' is not in the allowed range [0 .. 1]", fraction, name);
        return false;
    }
    auto it = _landCoverMap.find(string(name));
    if (it == _landCoverMap.end()) {
        LogError("Land cover '{}' was not found.", name);
        return false;
    }
    LandCover* changed = it->second;

    // Conserve the stored water by transferring the content sitting on the land that
    // changes hands between the changed cover and the generic (soil) cover that
    // absorbs the area difference. Only relevant when there is a distinct generic
    // cover to exchange with.
    LandCover* generic = GetGenericLandCover();
    if (generic != nullptr && generic != changed && _landCoverBricks.size() > 1) {
        double oldFraction = changed->GetAreaFraction();
        double delta = fraction - oldFraction;
        double genericOldFraction = generic->GetAreaFraction();
        double genericNewFraction = genericOldFraction - delta;

        // The generic cover must be able to absorb the change (it cannot go negative).
        if (LessThan(genericNewFraction, 0, EPSILON_D)) {
            LogError(
                "The generic soil (open) land cover (%.20g) is not large enough to compensate "
                "the area change (%.20g) with error margin (%.20g).",
                genericOldFraction, delta, EPSILON_D);
            return false;
        }

        TransferLandCoverContent(changed, oldFraction, fraction, generic, genericOldFraction, genericNewFraction);
    }

    changed->SetAreaFraction(fraction);
    return FixLandCoverFractionsTotal();
}

LandCover* HydroUnit::GetGenericLandCover() const {
    // 'open' is the canonical name; 'ground'/'generic'/'generic_land_cover' are accepted
    // aliases kept for backward compatibility.
    for (const auto& name : {"open", "ground", "generic", "generic_land_cover"}) {
        auto it = _landCoverMap.find(name);
        if (it != _landCoverMap.end()) {
            return it->second;
        }
    }
    return nullptr;
}

void HydroUnit::TransferLandCoverContent(LandCover* changed, double oldFraction, double newFraction, LandCover* generic,
                                         double genericOldFraction, double genericNewFraction) {
    double delta = newFraction - oldFraction;
    if (NearlyZero(delta, EPSILON_D)) {
        return;
    }

    // The cover that loses area is the donor; the one that gains area is the receiver.
    // The strip of land that changes hands carries the donor's water column.
    LandCover* donor;
    LandCover* receiver;
    double fReceiverOld;
    double fReceiverNew;
    double fDonorNew;
    double strip;
    if (delta > 0) {
        // The changed cover grows, taking land from the generic cover.
        donor = generic;
        receiver = changed;
        fReceiverOld = oldFraction;
        fReceiverNew = newFraction;
        fDonorNew = genericNewFraction;
        strip = delta;
    } else {
        // The changed cover shrinks, giving land to the generic cover.
        donor = changed;
        receiver = generic;
        fReceiverOld = genericOldFraction;
        fReceiverNew = genericNewFraction;
        fDonorNew = newFraction;
        strip = -delta;
    }

    if (NearlyZero(fReceiverNew, EPSILON_D)) {
        return;  // The receiver grows, so this should not happen; guard against division by zero.
    }

    vector<ContentSlot> receiverSlots = GetContentSlots(receiver, this);
    vector<ContentSlot> donorSlots = GetContentSlots(donor, this);

    // Liquid water held in donor storages the receiver cannot mirror (e.g. a forest
    // canopy converting to bare ground) is folded into the receiver's own water so no
    // water is lost. Snow/ice without a matching receiver storage stays with the donor
    // (a shrinking glacier's ice is left to the glacier-evolution action to manage).
    double unmatchedDonorWater = 0;
    for (const auto& slot : donorSlots) {
        if (slot.type == ContentType::Water && !HasRole(receiverSlots, slot.role)) {
            unmatchedDonorWater += slot.brick->GetContent(ContentType::Water);
        }
    }

    for (const auto& slot : receiverSlots) {
        double donorDepth = GetSlotDepth(donorSlots, slot.role);
        if (slot.role == CoverRole::SelfWater) {
            donorDepth += unmatchedDonorWater;
        }
        double receiverDepth = slot.brick->GetContent(slot.type);
        double newDepth = (receiverDepth * fReceiverOld + donorDepth * strip) / fReceiverNew;
        slot.brick->UpdateContent(newDepth, slot.type);
    }

    // If the donor cover has fully vanished, clear its now zero-area storages so no
    // stale depth lingers; its volume has already been credited to the receiver.
    if (NearlyZero(fDonorNew, EPSILON_D)) {
        for (const auto& slot : donorSlots) {
            slot.brick->UpdateContent(0, slot.type);
        }
    }
}

bool HydroUnit::FixLandCoverFractionsTotal() {
    // The generic (soil) land cover absorbs the area changes.
    LandCover* ground = GetGenericLandCover();

    double total = 0;
    for (auto brick : _landCoverBricks) {
        total += brick->GetAreaFraction();
    }

    if (!NearlyEqual(total, 1.0, EPSILON_D)) {
        double diff = total - 1.0;
        if (ground == nullptr) {
            LogError("No generic soil (open) land cover found. Cannot fix the land cover fractions.");
            return false;
        }
        if (LessThan(ground->GetAreaFraction(), diff, EPSILON_D)) {
            LogError(
                "The generic soil (open) land cover (%.20g) is not large enough to compensate "
                "the area fractions (%.20g) with error margin (%.20g)."
                "(i.e. the sum of the other land cover fractions is too large).",
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
