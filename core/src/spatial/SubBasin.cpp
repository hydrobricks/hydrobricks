#include "SubBasin.h"

#include "LandCover.h"
#include "SettingsBasin.h"
#include "SurfaceComponent.h"

SubBasin::SubBasin()
    : _area(0),
      _outletTotal(0),
      _needsCleanup(false) {}

SubBasin::~SubBasin() {
    if (_needsCleanup) {
        for (auto& hydroUnit : _hydroUnits) {
            wxDELETE(hydroUnit);
        }
    }
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
    _needsCleanup = true;

    // Pre-reserve containers when counts are known.
    ReserveHydroUnits(basinSettings.GetHydroUnitCount());

    // Create the hydro units
    for (int iUnit = 0; iUnit < basinSettings.GetHydroUnitCount(); ++iUnit) {
        basinSettings.SelectUnit(iUnit);

        HydroUnitSettings unitSettings = basinSettings.GetHydroUnitSettings(iUnit);
        auto unit = new HydroUnit(unitSettings.area);
        unit->SetProperties(unitSettings);
        AddHydroUnit(unit);
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
        for (int iUnit = 0; iUnit < basinSettings.GetHydroUnitCount(); ++iUnit) {
            basinSettings.SelectUnit(iUnit);

            for (int iElement = 0; iElement < basinSettings.GetLandCoverCount(); ++iElement) {
                LandCoverSettings elementSettings = basinSettings.GetLandCoverSettings(iElement);

                auto brick = dynamic_cast<LandCover*>(_hydroUnits[iUnit]->GetBrick(elementSettings.name));
                wxASSERT(brick);
                brick->SetAreaFraction(elementSettings.fraction);
            }

            for (int iElement = 0; iElement < basinSettings.GetSurfaceComponentCount(); ++iElement) {
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
    for (auto brick : _bricks) {
        brick->Reset();
    }
    for (auto hydroUnit : _hydroUnits) {
        hydroUnit->Reset();
    }
    for (auto flux : _outletFluxes) {
        flux->Reset();
    }
}

void SubBasin::SaveAsInitialState() {
    for (auto brick : _bricks) {
        brick->Reset();
    }
    for (auto hydroUnit : _hydroUnits) {
        hydroUnit->Reset();
    }
}

bool SubBasin::IsOk() {
    if (_hydroUnits.empty()) {
        wxLogError(_("The sub basin has no hydro unit attached."));
        return false;
    }
    for (auto unit : _hydroUnits) {
        if (!unit->IsOk()) return false;
    }
    for (auto brick : _bricks) {
        if (!brick->IsOk()) return false;
    }
    for (auto splitter : _splitters) {
        if (!splitter->IsOk()) return false;
    }

    return true;
}

void SubBasin::AddBrick(Brick* brick) {
    wxASSERT(brick);
    _bricks.push_back(brick);
}

void SubBasin::AddSplitter(Splitter* splitter) {
    wxASSERT(splitter);
    _splitters.push_back(splitter);
}

void SubBasin::AddHydroUnit(HydroUnit* unit) {
    _hydroUnits.push_back(unit);
    _area += unit->GetArea();
}

int SubBasin::GetHydroUnitCount() {
    return static_cast<int>(_hydroUnits.size());
}

HydroUnit* SubBasin::GetHydroUnit(int index) {
    wxASSERT(_hydroUnits.size() > index);
    wxASSERT(_hydroUnits[index]);

    return _hydroUnits[index];
}

HydroUnit* SubBasin::GetHydroUnitById(int id) {
    for (auto unit : _hydroUnits) {
        if (unit->GetId() == id) {
            return unit;
        }
    }
    wxLogError(_("The hydro unit %d was not found"), id);
    return nullptr;
}

vecInt SubBasin::GetHydroUnitIds() {
    vecInt ids;
    ids.reserve(_hydroUnits.size());
    for (auto unit : _hydroUnits) {
        ids.push_back(unit->GetId());
    }

    return ids;
}

vecDouble SubBasin::GetHydroUnitAreas() {
    vecDouble areas;
    areas.reserve(_hydroUnits.size());
    for (auto unit : _hydroUnits) {
        areas.push_back(unit->GetArea());
    }

    return areas;
}

int SubBasin::GetBricksCount() {
    return static_cast<int>(_bricks.size());
}

int SubBasin::GetSplittersCount() {
    return static_cast<int>(_splitters.size());
}

Brick* SubBasin::GetBrick(int index) {
    wxASSERT(_bricks.size() > index);
    wxASSERT(_bricks[index]);

    return _bricks[index];
}

bool SubBasin::HasBrick(const string& name) {
    for (auto brick : _bricks) {
        if (brick->GetName() == name) {
            return true;
        }
    }
    return false;
}

Brick* SubBasin::GetBrick(const string& name) {
    for (auto brick : _bricks) {
        if (brick->GetName() == name) {
            return brick;
        }
    }

    throw NotFound(wxString::Format(_("No brick with the name '%s' was found."), name));
}

Splitter* SubBasin::GetSplitter(int index) {
    wxASSERT(_splitters.size() > index);
    wxASSERT(_splitters[index]);

    return _splitters[index];
}

bool SubBasin::HasSplitter(const string& name) {
    for (auto splitter : _splitters) {
        if (splitter->GetName() == name) {
            return true;
        }
    }
    return false;
}

Splitter* SubBasin::GetSplitter(const string& name) {
    for (auto splitter : _splitters) {
        if (splitter->GetName() == name) {
            return splitter;
        }
    }

    throw NotFound(wxString::Format(_("No splitter with the name '%s' was found."), name));
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

bool SubBasin::HasIncomingFlow() {
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
