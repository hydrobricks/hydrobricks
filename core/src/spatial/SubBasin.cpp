#include "SubBasin.h"

#include <memory>
#include <utility>

#include "LandCover.h"
#include "SettingsBasin.h"
#include "SurfaceComponent.h"

SubBasin::SubBasin()
    : _area(0),
      _outletTotal(0) {}

SubBasin::~SubBasin() {
    // Unique_ptr members clean up owned objects automatically.
}

bool SubBasin::Initialize(SettingsBasin& basinSettings) {
    try {
        BuildBasin(basinSettings);
    } catch (const std::exception& e) {
        wxLogError(_("An exception occurred during basin initialization: %s."), e.what());
        return false;
    }

    return true;
}

void SubBasin::BuildBasin(SettingsBasin& basinSettings) {
    // Pre-reserve containers when counts are known.
    int hydroUnitCount = basinSettings.GetHydroUnitCount();
    ReserveHydroUnits(hydroUnitCount);

    // Create the hydro units
    for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
        basinSettings.SelectUnit(iUnit);

        HydroUnitSettings unitSettings = basinSettings.GetHydroUnitSettings(iUnit);
        auto unit = std::make_unique<HydroUnit>(unitSettings.area);
        unit->SetProperties(unitSettings);
        AddHydroUnit(std::move(unit));
    }

    // Create the lateral connections
    ReserveLateralConnectionsForUnits(basinSettings);
    for (const auto& connection : basinSettings.GetLateralConnections()) {
        HydroUnit* giver = GetHydroUnitById(connection.giverHydroUnitId);
        HydroUnit* receiver = GetHydroUnitById(connection.receiverHydroUnitId);

        if (giver && receiver) {
            giver->AddLateralConnection(receiver, connection.fraction, connection.type);
        } else {
            wxLogError(_("Invalid hydro unit IDs in lateral connection settings."));
        }
    }
}

bool SubBasin::AssignFractions(SettingsBasin& basinSettings) {
    try {
        int hydroUnitCount = basinSettings.GetHydroUnitCount();
        int landCoverCount = basinSettings.GetLandCoverCount();
        int surfaceComponentCount = basinSettings.GetSurfaceComponentCount();

        for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
            basinSettings.SelectUnit(iUnit);

            for (int iElement = 0; iElement < landCoverCount; ++iElement) {
                LandCoverSettings elementSettings = basinSettings.GetLandCoverSettings(iElement);

                auto brick = dynamic_cast<LandCover*>(_hydroUnits[iUnit]->GetBrick(elementSettings.name));
                wxASSERT(brick);
                brick->SetAreaFraction(elementSettings.fraction);
            }

            for (int iElement = 0; iElement < surfaceComponentCount; ++iElement) {
                SurfaceComponentSettings elementSettings = basinSettings.GetSurfaceComponentSettings(iElement);

                auto brick = dynamic_cast<SurfaceComponent*>(_hydroUnits[iUnit]->GetBrick(elementSettings.name));
                wxASSERT(brick);
                brick->SetAreaFraction(elementSettings.fraction);
            }
        }
    } catch (const std::exception& e) {
        wxLogError(_("An exception occurred while assigning the fractions: %s."), e.what());
        return false;
    }

    return true;
}

void SubBasin::Reset() {
    for (const auto& brick : _bricks) {
        brick->Reset();
    }
    for (const auto& hydroUnit : _hydroUnits) {
        hydroUnit->Reset();
    }
    for (auto flux : _outletFluxes) {
        flux->Reset();
    }
}

void SubBasin::SaveAsInitialState() {
    for (const auto& brick : _bricks) {
        brick->Reset();
    }
    for (const auto& hydroUnit : _hydroUnits) {
        hydroUnit->Reset();
    }
}

bool SubBasin::IsOk() const {
    if (_hydroUnits.empty()) {
        wxLogError(_("The sub basin has no hydro unit attached."));
        return false;
    }
    for (const auto& unit : _hydroUnits) {
        if (!unit->IsOk()) return false;
    }
    for (const auto& brick : _bricks) {
        if (!brick->IsOk()) return false;
    }
    for (const auto& splitter : _splitters) {
        if (!splitter->IsOk()) return false;
    }

    return true;
}

void SubBasin::AddBrick(std::unique_ptr<Brick> brick) {
    wxASSERT(brick);
    Brick* rawBrick = brick.get();
    _brickMap[rawBrick->GetName()] = rawBrick;
    _bricks.push_back(std::move(brick));
}

void SubBasin::AddSplitter(std::unique_ptr<Splitter> splitter) {
    wxASSERT(splitter);
    Splitter* rawSplitter = splitter.get();
    _splitterMap[rawSplitter->GetName()] = rawSplitter;
    _splitters.push_back(std::move(splitter));
}

void SubBasin::AddHydroUnit(std::unique_ptr<HydroUnit> unit) {
    wxASSERT(unit);
    HydroUnit* rawUnit = unit.get();
    _hydroUnitMap[rawUnit->GetId()] = rawUnit;
    _area += rawUnit->GetArea();
    _hydroUnits.push_back(std::move(unit));
}

int SubBasin::GetHydroUnitCount() const {
    return static_cast<int>(_hydroUnits.size());
}

HydroUnit* SubBasin::GetHydroUnit(size_t index) const {
    wxASSERT(_hydroUnits.size() > index);
    wxASSERT(_hydroUnits[index]);

    return _hydroUnits[index].get();
}

HydroUnit* SubBasin::GetHydroUnitById(int id) const {
    auto it = _hydroUnitMap.find(id);
    if (it != _hydroUnitMap.end()) {
        return it->second;
    }
    wxLogError(_("The hydro unit %d was not found"), id);
    return nullptr;
}

vecInt SubBasin::GetHydroUnitIds() const {
    vecInt ids;
    ids.reserve(_hydroUnits.size());
    for (const auto& unit : _hydroUnits) {
        ids.push_back(unit->GetId());
    }

    return ids;
}

vecDouble SubBasin::GetHydroUnitAreas() const {
    vecDouble areas;
    areas.reserve(_hydroUnits.size());
    for (const auto& unit : _hydroUnits) {
        areas.push_back(unit->GetArea());
    }

    return areas;
}

int SubBasin::GetBricksCount() const {
    return static_cast<int>(_bricks.size());
}

int SubBasin::GetSplittersCount() const {
    return static_cast<int>(_splitters.size());
}

Brick* SubBasin::GetBrick(size_t index) const {
    wxASSERT(_bricks.size() > index);
    wxASSERT(_bricks[index]);

    return _bricks[index].get();
}

bool SubBasin::HasBrick(const string& name) const {
    return _brickMap.find(name) != _brickMap.end();
}

Brick* SubBasin::GetBrick(const string& name) const {
    auto it = _brickMap.find(name);
    if (it != _brickMap.end()) {
        return it->second;
    }

    throw ModelConfigError(wxString::Format(_("No brick with the name '%s' was found."), name));
}

Splitter* SubBasin::GetSplitter(size_t index) const {
    wxASSERT(_splitters.size() > index);
    wxASSERT(_splitters[index]);

    return _splitters[index].get();
}

bool SubBasin::HasSplitter(const string& name) const {
    return _splitterMap.find(name) != _splitterMap.end();
}

Splitter* SubBasin::GetSplitter(const string& name) const {
    auto it = _splitterMap.find(name);
    if (it != _splitterMap.end()) {
        return it->second;
    }

    throw ModelConfigError(wxString::Format(_("No splitter with the name '%s' was found."), name));
}

void SubBasin::ReserveLateralConnectionsForUnits(SettingsBasin& basinSettings) {
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

bool SubBasin::HasIncomingFlow() const {
    return !_inConnectors.empty();
}

void SubBasin::AddInputConnector(Connector* connector) {
    wxASSERT(connector);
    _inConnectors.push_back(connector);
}

void SubBasin::AddOutputConnector(Connector* connector) {
    wxASSERT(connector);
    _outConnectors.push_back(connector);
}

void SubBasin::AttachOutletFlux(Flux* flux) {
    wxASSERT(flux);
    _outletFluxes.push_back(flux);
}

double* SubBasin::GetValuePointer(const string& name) {
    if (name == "outlet") {
        return &_outletTotal;
    }
    wxLogError(_("Element '%s' not found"), name);

    return nullptr;
}

bool SubBasin::ComputeOutletDischarge() {
    _outletTotal = 0;
    for (auto flux : _outletFluxes) {
        _outletTotal += flux->GetAmount();
    }

    return true;
}
