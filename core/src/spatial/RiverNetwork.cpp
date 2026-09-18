#include "RiverNetwork.h"

#include <algorithm>

ModelResult RiverNetwork::Initialize(SettingsBasin& basinSettings) {
    if (auto r = basinSettings.ValidateNetwork(); !r) {
        return std::unexpected(std::format("Invalid river network: {}", r.error()));
    }

    try {
        Build(basinSettings);
        BuildProcessingOrder();
        ComputeDrainedAreas();
        BuildReaches();
    } catch (const std::exception& e) {
        return std::unexpected(std::format("River network initialization failed: {}", e.what()));
    }

    return {};
}

void RiverNetwork::Build(SettingsBasin& basinSettings) {
    _subbasins.clear();
    _subbasinMap.clear();
    _hydroUnitMap.clear();
    _order.clear();
    _outlet = nullptr;
    _reaches.clear();
    _upstream.clear();

    if (basinSettings.GetSubbasinCount() == 0) {
        // No declared network: one implicit sub basin (ID 1) holding every unit, built exactly as before.
        auto subbasin = std::make_unique<SubBasin>();
        subbasin->SetId(1);
        if (auto r = subbasin->Initialize(basinSettings); !r) {
            throw std::runtime_error(r.error());
        }
        _subbasinMap[1] = subbasin.get();
        _subbasins.push_back(std::move(subbasin));
    } else {
        _subbasins.reserve(basinSettings.GetSubbasinCount());
        for (const auto& settings : basinSettings.GetSubbasins()) {
            auto subbasin = std::make_unique<SubBasin>();
            subbasin->SetNetworkProperties(settings);
            _subbasinMap[settings.id] = subbasin.get();
            _subbasins.push_back(std::move(subbasin));
        }

        // Create the hydro units in their sub basins (the settings were validated: every ID exists).
        int hydroUnitCount = basinSettings.GetHydroUnitCount();
        for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
            basinSettings.SelectUnit(iUnit);
            HydroUnitSettings unitSettings = basinSettings.GetHydroUnitSettings(iUnit);
            _subbasinMap.at(unitSettings.subbasinId)->AddHydroUnitFromSettings(unitSettings);
        }

        // Lateral connections between hydro units may cross sub basin boundaries: index the units first.
        for (const auto& subbasin : _subbasins) {
            for (int iUnit = 0; iUnit < subbasin->GetHydroUnitCount(); ++iUnit) {
                HydroUnit* unit = subbasin->GetHydroUnit(iUnit);
                _hydroUnitMap[unit->GetId()] = unit;
            }
        }
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

    for (const auto& subbasin : _subbasins) {
        for (int iUnit = 0; iUnit < subbasin->GetHydroUnitCount(); ++iUnit) {
            HydroUnit* unit = subbasin->GetHydroUnit(iUnit);
            _hydroUnitMap[unit->GetId()] = unit;
        }
        if (subbasin->IsTerminal()) {
            _outlet = subbasin.get();
        }
    }

    // Upstream neighbours of every sub basin, looked up on every time step.
    for (const auto& subbasin : _subbasins) {
        _upstream[subbasin.get()] = {};
    }
    for (const auto& subbasin : _subbasins) {
        if (!subbasin->IsTerminal()) {
            _upstream[_subbasinMap.at(subbasin->GetDownstreamId())].push_back(subbasin.get());
        }
    }
    for (const auto& subbasin : _subbasins) {
        subbasin->SetHasUpstream(!_upstream.at(subbasin.get()).empty());
    }
}

void RiverNetwork::BuildReaches() {
    _reaches.reserve(_subbasins.size());
    for (const auto& subbasin : _subbasins) {
        auto reach = std::make_unique<Reach>(subbasin.get());
        reach->Initialize();
        subbasin->SetReach(reach.get());
        _reaches.push_back(std::move(reach));
    }
}

void RiverNetwork::TransferInflow(SubBasin* subbasin, double timeStepInDays) {
    double inflowVolume = 0;
    for (const SubBasin* upstream : _upstream.at(subbasin)) {
        inflowVolume += upstream->GetOutletVolume();
    }
    subbasin->SetInflowVolume(subbasin->GetReach()->Route(inflowVolume, timeStepInDays));
}

ModelResult RiverNetwork::AssignFractions(SettingsBasin& basinSettings) {
    for (const auto& subbasin : _subbasins) {
        if (auto r = subbasin->AssignFractions(basinSettings); !r) {
            return r;
        }
    }
    return {};
}

void RiverNetwork::Reset() {
    for (const auto& reach : _reaches) {
        reach->Reset();
    }
}

void RiverNetwork::SaveAsInitialState() {
    for (const auto& reach : _reaches) {
        reach->SaveAsInitialState();
    }
}

Reach* RiverNetwork::GetReach(size_t index) const {
    assert(index < _reaches.size());
    return _reaches[index].get();
}

void RiverNetwork::BuildProcessingOrder() {
    // Post-order traversal from the outlet: a sub basin is emitted once all its upstream neighbours are.
    _order.clear();
    _order.reserve(_subbasins.size());

    std::vector<std::pair<SubBasin*, bool>> stack;  // (sub basin, upstream already pushed)
    stack.emplace_back(_outlet, false);
    while (!stack.empty()) {
        auto [subbasin, expanded] = stack.back();
        stack.pop_back();
        if (expanded) {
            _order.push_back(subbasin);
            continue;
        }
        stack.emplace_back(subbasin, true);
        for (SubBasin* upstream : GetUpstreamSubbasins(subbasin)) {
            stack.emplace_back(upstream, false);
        }
    }

    if (_order.size() != _subbasins.size()) {
        throw ModelConfigError("The river network is not a single tree: some sub basins do not reach the outlet.");
    }
}

void RiverNetwork::ComputeDrainedAreas() {
    for (const auto& subbasin : _subbasins) {
        subbasin->SetDrainedArea(0);
    }
    // Upstream first: when a sub basin is reached, every contribution from upstream is already summed.
    for (SubBasin* subbasin : _order) {
        subbasin->SetDrainedArea(subbasin->GetDrainedArea() + subbasin->GetLocalArea());
        if (!subbasin->IsTerminal()) {
            SubBasin* downstream = _subbasinMap.at(subbasin->GetDownstreamId());
            downstream->SetDrainedArea(downstream->GetDrainedArea() + subbasin->GetDrainedArea());
        }
    }
}

SubBasin* RiverNetwork::GetSubbasin(size_t index) const {
    assert(index < _subbasins.size());
    return _subbasins[index].get();
}

SubBasin* RiverNetwork::GetSubbasinById(int id) const {
    auto it = _subbasinMap.find(id);
    if (it != _subbasinMap.end()) {
        return it->second;
    }
    return nullptr;
}

const std::vector<SubBasin*>& RiverNetwork::GetUpstreamSubbasins(const SubBasin* subbasin) const {
    return _upstream.at(subbasin);
}

HydroUnit* RiverNetwork::GetHydroUnitById(int id) const {
    auto it = _hydroUnitMap.find(id);
    if (it != _hydroUnitMap.end()) {
        return it->second;
    }
    LogError("The hydro unit {} was not found in the river network", id);
    return nullptr;
}

int RiverNetwork::GetHydroUnitCount() const {
    int count = 0;
    for (const auto& subbasin : _subbasins) {
        count += subbasin->GetHydroUnitCount();
    }
    return count;
}

vecInt RiverNetwork::GetSubbasinIds() const {
    vecInt ids;
    ids.reserve(_subbasins.size());
    for (const auto& subbasin : _subbasins) {
        ids.push_back(subbasin->GetId());
    }
    return ids;
}

vecInt RiverNetwork::GetSubbasinDownstreamIds() const {
    vecInt ids;
    ids.reserve(_subbasins.size());
    for (const auto& subbasin : _subbasins) {
        ids.push_back(subbasin->GetDownstreamId());
    }
    return ids;
}

vecDouble RiverNetwork::GetSubbasinLocalAreas() const {
    vecDouble areas;
    areas.reserve(_subbasins.size());
    for (const auto& subbasin : _subbasins) {
        areas.push_back(subbasin->GetLocalArea());
    }
    return areas;
}

vecDouble RiverNetwork::GetSubbasinDrainedAreas() const {
    vecDouble areas;
    areas.reserve(_subbasins.size());
    for (const auto& subbasin : _subbasins) {
        areas.push_back(subbasin->GetDrainedArea());
    }
    return areas;
}

double RiverNetwork::GetTotalArea() const {
    return _outlet ? _outlet->GetDrainedArea() : 0.0;
}
