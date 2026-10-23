#include "Subbasin.h"

#include <memory>
#include <utility>

#include "LandCover.h"
#include "Reach.h"
#include "SettingsBasin.h"
#include "SurfaceComponent.h"

Subbasin::Subbasin()
    : _area(0),
      _outletTotal(0) {}

Subbasin::~Subbasin() {
    // Unique_ptr members clean up owned objects automatically.
}

ModelResult Subbasin::Initialize(SettingsBasin& basinSettings) {
    try {
        BuildBasin(basinSettings);
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Basin initialization failed: {}", e.what()));
    }

    return {};
}

void Subbasin::BuildBasin(SettingsBasin& basinSettings) {
    // Pre-reserve containers when counts are known.
    int hydroUnitCount = basinSettings.GetHydroUnitCount();
    ReserveHydroUnits(hydroUnitCount);

    // Create the hydro units
    for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
        basinSettings.SelectUnit(iUnit);

        HydroUnitSettings unitSettings = basinSettings.GetHydroUnitSettings(iUnit);
        AddHydroUnitFromSettings(unitSettings);
    }

    // Create the lateral connections
    ReserveLateralConnectionsForUnits(basinSettings);
    for (const auto& connection : basinSettings.GetLateralConnections()) {
        HydroUnit* giver = GetHydroUnitById(connection.giverHydroUnitId);
        HydroUnit* receiver = GetHydroUnitById(connection.receiverHydroUnitId);

        if (giver && receiver) {
            giver->AddLateralConnection(receiver, connection.fraction, connection.type);
        } else {
            LogError("Invalid hydro unit IDs in lateral connection settings.");
        }
    }
}

HydroUnit* Subbasin::AddHydroUnitFromSettings(HydroUnitSettings& unitSettings) {
    auto unit = std::make_unique<HydroUnit>(unitSettings.area);
    unit->SetProperties(unitSettings);
    HydroUnit* rawUnit = unit.get();
    AddHydroUnit(std::move(unit));

    return rawUnit;
}

void Subbasin::SetNetworkProperties(const SubbasinSettings& subbasinSettings) {
    _id = subbasinSettings.id;
    _downstreamId = subbasinSettings.downstreamId;
    _name = subbasinSettings.name;

    _properties.reserve(_properties.size() + subbasinSettings.propertiesDouble.size() +
                        subbasinSettings.propertiesString.size());
    for (const auto& property : subbasinSettings.propertiesDouble) {
        _properties.push_back(std::make_unique<HydroUnitProperty>(property.name, property.value, property.unit));
    }
    for (const auto& property : subbasinSettings.propertiesString) {
        _properties.push_back(std::make_unique<HydroUnitProperty>(property.name, property.value));
    }
}

bool Subbasin::HasProperty(std::string_view name) const {
    for (const auto& property : _properties) {
        if (property->GetName() == name) {
            return true;
        }
    }

    return false;
}

double Subbasin::GetPropertyDouble(std::string_view name, std::string_view unit) const {
    for (const auto& property : _properties) {
        if (property->GetName() == name) {
            return property->GetValue(unit);
        }
    }

    throw ModelConfigError(std::format("No property with the name '{}' was found on subbasin {}.", name, _id));
}

string Subbasin::GetPropertyString(std::string_view name) const {
    for (const auto& property : _properties) {
        if (property->GetName() == name) {
            return property->GetValueString();
        }
    }

    throw ModelConfigError(std::format("No property with the name '{}' was found on subbasin {}.", name, _id));
}

ModelResult Subbasin::AssignFractions(SettingsBasin& basinSettings) {
    try {
        int hydroUnitCount = basinSettings.GetHydroUnitCount();

        for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
            basinSettings.SelectUnit(iUnit);

            // The settings list every unit of the catchment; only those of this subbasin are handled here.
            int unitId = basinSettings.GetHydroUnitSettings(iUnit).id;
            if (!HasHydroUnit(unitId)) {
                continue;
            }
            HydroUnit* unit = GetHydroUnitById(unitId);

            // Per-unit counts: units may carry different land covers (structure variants).
            int landCoverCount = basinSettings.GetLandCoverCount();
            int surfaceComponentCount = basinSettings.GetSurfaceComponentCount();

            for (int iElement = 0; iElement < landCoverCount; ++iElement) {
                LandCoverSettings elementSettings = basinSettings.GetLandCoverSettings(iElement);

                // The unit's structure variant may not include this land cover (e.g. a
                // non-glacier unit on a glacier-free structure); skip it then.
                auto* genericBrick = unit->TryGetBrick(elementSettings.name);
                if (genericBrick == nullptr) {
                    continue;
                }
                auto brick = dynamic_cast<LandCover*>(genericBrick);
                assert(brick);
                brick->SetAreaFraction(elementSettings.fraction);
                // Capture the assigned extent as the initial one to restore on reset.
                brick->SetInitialAreaFraction(elementSettings.fraction);
            }

            for (int iElement = 0; iElement < surfaceComponentCount; ++iElement) {
                SurfaceComponentSettings elementSettings = basinSettings.GetSurfaceComponentSettings(iElement);

                auto* genericBrick = unit->TryGetBrick(elementSettings.name);
                if (genericBrick == nullptr) {
                    continue;
                }
                auto brick = dynamic_cast<SurfaceComponent*>(genericBrick);
                assert(brick);
                brick->SetAreaFraction(elementSettings.fraction);
            }
        }
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Fraction assignment failed: {}", e.what()));
    }

    return {};
}

void Subbasin::Reset() {
    for (const auto& brick : _bricks) {
        brick->Reset();
    }
    for (const auto& hydroUnit : _hydroUnits) {
        hydroUnit->Reset();
    }
    for (auto flux : _outletFluxes) {
        flux->Reset();
    }
    _outletTotal = 0;
    _inflowVolume = 0;
    _outletVolume = 0;
    _outletDischarge = 0;
    _reachInflow = 0;
    _reachOutflow = 0;
    _reachStorage = 0;
}

void Subbasin::SaveAsInitialState() {
    for (const auto& brick : _bricks) {
        brick->SaveAsInitialState();
    }
    for (const auto& hydroUnit : _hydroUnits) {
        hydroUnit->SaveAsInitialState();
    }
}

void Subbasin::RestoreInitialAreaFractions() {
    for (const auto& hydroUnit : _hydroUnits) {
        hydroUnit->RestoreInitialAreaFractions();
    }
}

bool Subbasin::IsValid(bool checkProcesses) const {
    if (_hydroUnits.empty()) {
        LogError("The subbasin has no hydro unit attached.");
        return false;
    }
    for (const auto& unit : _hydroUnits) {
        if (!unit->IsValid(checkProcesses)) return false;
    }
    for (const auto& brick : _bricks) {
        if (!brick->IsValid(checkProcesses)) return false;
    }
    for (const auto& splitter : _splitters) {
        if (!splitter->IsValid()) return false;
    }

    return true;
}

void Subbasin::Validate() const {
    if (!IsValid()) {
        throw ModelConfigError(
            "Subbasin validation failed. Check that hydro units, bricks, and splitters are properly configured.");
    }
}

void Subbasin::AddBrick(std::unique_ptr<Brick> brick) {
    assert(brick);
    Brick* rawBrick = brick.get();
    _brickMap[rawBrick->GetName()] = rawBrick;
    _bricks.push_back(std::move(brick));
}

void Subbasin::AddSplitter(std::unique_ptr<Splitter> splitter) {
    assert(splitter);
    Splitter* rawSplitter = splitter.get();
    _splitterMap[rawSplitter->GetName()] = rawSplitter;
    _splitters.push_back(std::move(splitter));
}

void Subbasin::AddHydroUnit(std::unique_ptr<HydroUnit> unit) {
    assert(unit);
    HydroUnit* rawUnit = unit.get();
    _hydroUnitMap[rawUnit->GetId()] = rawUnit;
    _area += rawUnit->GetArea();
    _hydroUnits.push_back(std::move(unit));
}

void Subbasin::ResetForcingUpdates() {
    for (const auto& unit : _hydroUnits) {
        unit->ResetForcingUpdates();
    }
}

int Subbasin::GetHydroUnitCount() const {
    return static_cast<int>(_hydroUnits.size());
}

HydroUnit* Subbasin::GetHydroUnit(size_t index) const {
    assert(_hydroUnits.size() > index);
    assert(_hydroUnits[index]);

    return _hydroUnits[index].get();
}

HydroUnit* Subbasin::GetHydroUnitById(int id) const {
    auto it = _hydroUnitMap.find(id);
    if (it != _hydroUnitMap.end()) {
        return it->second;
    }
    LogError("The hydro unit {} was not found", id);
    return nullptr;
}

vecInt Subbasin::GetHydroUnitIds() const {
    vecInt ids;
    ids.reserve(_hydroUnits.size());
    for (const auto& unit : _hydroUnits) {
        ids.push_back(unit->GetId());
    }

    return ids;
}

vecDouble Subbasin::GetHydroUnitAreas() const {
    vecDouble areas;
    areas.reserve(_hydroUnits.size());
    for (const auto& unit : _hydroUnits) {
        areas.push_back(unit->GetArea());
    }

    return areas;
}

vecInt Subbasin::GetHydroUnitStructureIds() const {
    vecInt ids;
    ids.reserve(_hydroUnits.size());
    for (const auto& unit : _hydroUnits) {
        ids.push_back(unit->GetStructureId());
    }

    return ids;
}

int Subbasin::GetBrickCount() const {
    return static_cast<int>(_bricks.size());
}

int Subbasin::GetSplitterCount() const {
    return static_cast<int>(_splitters.size());
}

Brick* Subbasin::GetBrick(size_t index) const {
    assert(_bricks.size() > index);
    assert(_bricks[index]);

    return _bricks[index].get();
}

bool Subbasin::HasBrick(std::string_view name) const {
    return _brickMap.find(string(name)) != _brickMap.end();
}

Brick* Subbasin::GetBrick(std::string_view name) const {
    auto it = _brickMap.find(string(name));
    if (it != _brickMap.end()) {
        return it->second;
    }

    throw ModelConfigError(std::format("No brick with the name '{}' was found.", name));
}

Splitter* Subbasin::GetSplitter(size_t index) const {
    assert(_splitters.size() > index);
    assert(_splitters[index]);

    return _splitters[index].get();
}

bool Subbasin::HasSplitter(std::string_view name) const {
    return _splitterMap.find(string(name)) != _splitterMap.end();
}

Splitter* Subbasin::GetSplitter(std::string_view name) const {
    auto it = _splitterMap.find(string(name));
    if (it != _splitterMap.end()) {
        return it->second;
    }

    throw ModelConfigError(std::format("No splitter with the name '{}' was found.", name));
}

void Subbasin::ReserveLateralConnectionsForUnits(SettingsBasin& basinSettings) {
    // Pre-count connections per giver to reserve in each hydro unit.
    std::unordered_map<int, size_t> counts;
    for (const auto& connection : basinSettings.GetLateralConnections()) {
        counts[connection.giverHydroUnitId]++;
    }
    for (const auto& [giverId, cnt] : counts) {
        if (auto* giver = GetHydroUnitById(giverId)) {
            giver->ReserveLateralConnections(cnt);
        }
    }
}

bool Subbasin::HasIncomingFlow() const {
    return !_inConnectors.empty();
}

void Subbasin::AddInputConnector(Connector* connector) {
    assert(connector);
    _inConnectors.push_back(connector);
}

void Subbasin::AddOutputConnector(Connector* connector) {
    assert(connector);
    _outConnectors.push_back(connector);
}

void Subbasin::AttachOutletFlux(Flux* flux) {
    assert(flux);
    _outletFluxes.push_back(flux);
}

double* Subbasin::GetValuePointer(std::string_view name) {
    if (name == "outlet") {
        return &_outletDischarge;
    }
    if (name == "reach:inflow") {
        return &_reachInflow;
    }
    if (name == "reach:outflow") {
        return &_reachOutflow;
    }
    if (name == "reach:storage") {
        return &_reachStorage;
    }
    LogError("Element '{}' not found", name);

    return nullptr;
}

bool Subbasin::ComputeOutletDischarge(double timeStepInDays) {
    // Own runoff, in mm over the local area (the outlet fluxes are pre-weighted by unit area / local area).
    _outletTotal = 0;
    for (auto flux : _outletFluxes) {
        _outletTotal += flux->GetAmount();
    }

    // The own runoff travels through half the reach when the channel routing routes it (it is generated
    // along the reach), else it joins the outlet directly.
    double localVolume = _outletTotal * _area;
    bool routesLocal = _reach != nullptr && _reach->RoutesLocalRunoff();
    if (routesLocal) {
        localVolume = _reach->RouteLocal(localVolume, timeStepInDays);
    }

    // At the outlet, the own runoff joins the volume routed from upstream (set by the river network before
    // this subbasin is processed). The discharge reported at the outlet is expressed over the drained area,
    // which is what a gauge normalized by its catchment area shows. Without upstream inflow nor local routing
    // it is the local total itself (assigned, not recomputed, so single-subbasin runs stay bit-identical).
    _outletVolume = localVolume + _inflowVolume;
    _outletDischarge = (_hasUpstream || routesLocal) ? _outletVolume / _drainedArea : _outletTotal;

    // The reach values, logged in the same unit as the outlet (mm over the drained area).
    if (_reach != nullptr && _hasUpstream) {
        _reachInflow = _reach->GetInflow() / _drainedArea;
        _reachOutflow = _reach->GetOutflow() / _drainedArea;
        _reachStorage = _reach->GetStorage() / _drainedArea;
    }

    return true;
}
