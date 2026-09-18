#include "SettingsBasin.h"

#include <unordered_map>

#include "FileNetcdf.h"
#include "Parameter.h"

SettingsBasin::SettingsBasin()
    : _selectedHydroUnit(nullptr) {}

SettingsBasin::~SettingsBasin() = default;

void SettingsBasin::AddHydroUnit(int id, double area, double elevation, int subbasinId) {
    HydroUnitSettings unit;
    unit.id = id;
    unit.area = area;
    unit.elevation = elevation;
    unit.subbasinId = subbasinId;
    _hydroUnits.push_back(unit);
    _selectedHydroUnit = &_hydroUnits[_hydroUnits.size() - 1];
}

void SettingsBasin::AddSubbasin(int id, int downstreamId, const string& name) {
    SubbasinSettings subbasin;
    subbasin.id = id;
    subbasin.downstreamId = downstreamId;
    subbasin.name = name;
    _subbasins.push_back(subbasin);
    _selectedSubbasin = &_subbasins[_subbasins.size() - 1];
}

void SettingsBasin::AddSubbasinPropertyDouble(const string& name, double value, const string& unit) {
    assert(_selectedSubbasin);

    HydroUnitPropertyDouble property;
    property.name = name;
    property.value = value;
    property.unit = unit;
    _selectedSubbasin->propertiesDouble.push_back(property);
}

void SettingsBasin::AddSubbasinPropertyString(const string& name, const string& value) {
    assert(_selectedSubbasin);

    HydroUnitPropertyString property;
    property.name = name;
    property.value = value;
    _selectedSubbasin->propertiesString.push_back(property);
}

void SettingsBasin::SelectSubbasin(int index) {
    assert(_subbasins.size() > index);
    _selectedSubbasin = &_subbasins[index];
}

ModelResult SettingsBasin::ValidateNetwork() const {
    // No declared network: the implicit single subbasin 1 holds every unit.
    if (_subbasins.empty()) {
        for (const auto& unit : _hydroUnits) {
            if (unit.subbasinId != 1) {
                return std::unexpected(
                    std::format("Hydro unit {} refers to subbasin {}, but no subbasin is declared (declare the "
                                "subbasins or leave the units on subbasin 1).",
                                unit.id, unit.subbasinId));
            }
        }
        return {};
    }

    std::unordered_map<int, int> positions;  // subbasin id -> index in _subbasins
    for (int i = 0; i < static_cast<int>(_subbasins.size()); ++i) {
        int id = _subbasins[i].id;
        if (id <= 0) {
            return std::unexpected(std::format("Subbasin IDs must be positive integers (found {}).", id));
        }
        if (positions.contains(id)) {
            return std::unexpected(std::format("Subbasin ID {} is declared twice.", id));
        }
        positions[id] = i;
    }

    int rootCount = 0;
    for (const auto& subbasin : _subbasins) {
        if (subbasin.downstreamId == 0) {
            rootCount++;
        } else if (!positions.contains(subbasin.downstreamId)) {
            return std::unexpected(
                std::format("Subbasin {} drains to the undeclared subbasin {}.", subbasin.id, subbasin.downstreamId));
        }
    }
    if (rootCount != 1) {
        return std::unexpected(std::format(
            "The river network must have exactly one terminal subbasin (downstream ID 0); found {}.", rootCount));
    }

    // Cycles: following the downstream links from any subbasin must reach the outlet within n steps.
    int count = static_cast<int>(_subbasins.size());
    for (const auto& subbasin : _subbasins) {
        int current = subbasin.id;
        for (int step = 0; step <= count; ++step) {
            int downstream = _subbasins[positions[current]].downstreamId;
            if (downstream == 0) {
                break;
            }
            if (downstream == subbasin.id || step == count) {
                return std::unexpected(std::format("The river network has a cycle through subbasin {}.", subbasin.id));
            }
            current = downstream;
        }
    }

    vector<int> unitCounts(_subbasins.size(), 0);
    for (const auto& unit : _hydroUnits) {
        auto it = positions.find(unit.subbasinId);
        if (it == positions.end()) {
            return std::unexpected(
                std::format("Hydro unit {} refers to the undeclared subbasin {}.", unit.id, unit.subbasinId));
        }
        unitCounts[it->second]++;
    }
    for (int i = 0; i < count; ++i) {
        if (unitCounts[i] == 0) {
            return std::unexpected(std::format("Subbasin {} has no hydro unit.", _subbasins[i].id));
        }
    }

    return {};
}

void SettingsBasin::AddLandCover(const string& name, const string& type, double fraction) {
    assert(_selectedHydroUnit);

    LandCoverSettings element;
    element.name = name;
    element.type = type;
    element.fraction = fraction;
    _selectedHydroUnit->landCovers.push_back(element);
}

void SettingsBasin::AddHydroUnitPropertyDouble(const string& name, double value, const string& unit) {
    assert(_selectedHydroUnit);

    HydroUnitPropertyDouble property;
    property.name = name;
    property.value = value;
    property.unit = unit;
    _selectedHydroUnit->propertiesDouble.push_back(property);
}

void SettingsBasin::AddHydroUnitPropertyString(const string& name, const string& value) {
    assert(_selectedHydroUnit);

    HydroUnitPropertyString property;
    property.name = name;
    property.value = value;
    _selectedHydroUnit->propertiesString.push_back(property);
}

void SettingsBasin::AddLateralConnection(int giverHydroUnitId, int receiverHydroUnitId, double fraction,
                                         const string& type) {
    LateralConnectionSettings connection;
    connection.type = type;
    connection.giverHydroUnitId = giverHydroUnitId;
    connection.receiverHydroUnitId = receiverHydroUnitId;
    connection.fraction = fraction;

    _lateralConnections.push_back(connection);
}

void SettingsBasin::Clear() {
    _hydroUnits.clear();
    _selectedHydroUnit = nullptr;
    _subbasins.clear();
    _selectedSubbasin = nullptr;
}

void SettingsBasin::SelectUnit(int index) {
    assert(_hydroUnits.size() > index);
    _selectedHydroUnit = &_hydroUnits[index];
}

bool SettingsBasin::Parse(const string& path) {
    try {
        FileNetcdf file;

        if (!file.OpenReadOnly(path)) {
            return false;
        }

        // Get the land cover names
        vecStr landCovers;
        if (file.HasAtt("land_covers")) {
            landCovers = file.GetAttString1D("land_covers");
        } else if (file.HasAtt("surface_names")) {
            landCovers = file.GetAttString1D("surface_names");
        }

        // Get number of units
        int unitCount = file.GetDimLen("hydro_units");

        // Get ids
        vecInt ids = file.GetVarInt1D("id", unitCount);

        // Get areas
        vecDouble areas = file.GetVarDouble1D("area", unitCount);

        // Store hydro units
        for (int iUnit = 0; iUnit < unitCount; ++iUnit) {
            HydroUnitSettings unit;
            unit.id = ids[iUnit];
            unit.area = areas[iUnit];
            _hydroUnits.push_back(unit);
        }

        // Get land cover data
        for (const auto& cover : landCovers) {
            vecDouble fractions = file.GetVarDouble1D(cover, unitCount);

            // Get the cover type
            string type = file.GetAttText("type", cover);

            for (int iUnit = 0; iUnit < unitCount; ++iUnit) {
                LandCoverSettings element;
                element.name = cover;
                element.type = type;
                element.fraction = fractions[iUnit];
                _hydroUnits[iUnit].landCovers.push_back(element);
            }
        }

        // Get the subbasin of each unit (files without a network drain everything to subbasin 1)
        if (file.HasVar("subbasin")) {
            vecInt subbasinIds = file.GetVarInt1D("subbasin", unitCount);
            for (int iUnit = 0; iUnit < unitCount; ++iUnit) {
                _hydroUnits[iUnit].subbasinId = subbasinIds[iUnit];
            }
        }

        // Get the river network: one entry per subbasin, with its downstream link and properties
        if (file.HasDim("subbasins")) {
            ParseSubbasins(file);
        }

        // Extract other variables
        int varCount = file.GetVariableCount();
        int dimIdHydroUnits = file.GetDimId("hydro_units");

        for (int iVar = 0; iVar < varCount; ++iVar) {
            // Get variable name
            string varName = file.GetVarName(iVar);

            if (varName == "id" || varName == "area" || varName == "subbasin") {
                continue;
            }

            vecInt dimId = file.GetVarDimIds(iVar, 1);
            if (dimId[0] != dimIdHydroUnits) {
                continue;
            }

            vecDouble values = file.GetVarDouble1D(varName, unitCount);

            for (int iUnit = 0; iUnit < unitCount; ++iUnit) {
                HydroUnitPropertyDouble prop;
                prop.name = varName;
                prop.value = values[iUnit];
                prop.unit = "";
                _hydroUnits[iUnit].propertiesDouble.push_back(prop);
            }
        }

    } catch (std::exception& e) {
        LogError(e.what());
        return false;
    }

    return true;
}

void SettingsBasin::ParseSubbasins(const FileNetcdf& file) {
    int subbasinCount = file.GetDimLen("subbasins");
    vecInt ids = file.GetVarInt1D("subbasin_id", subbasinCount);
    vecInt downstreamIds = file.GetVarInt1D("subbasin_downstream_id", subbasinCount);
    vecStr names;
    if (file.HasAtt("subbasin_names")) {
        names = file.GetAttString1D("subbasin_names");
    }

    for (int i = 0; i < subbasinCount; ++i) {
        string name = static_cast<int>(names.size()) == subbasinCount ? names[i] : "";
        AddSubbasin(ids[i], downstreamIds[i], name);
    }

    // Any other 1-D variable on the subbasins dimension is a subbasin property (reach length, slope, ...).
    int varCount = file.GetVariableCount();
    int dimIdSubbasins = file.GetDimId("subbasins");

    for (int iVar = 0; iVar < varCount; ++iVar) {
        string varName = file.GetVarName(iVar);
        if (varName == "subbasin_id" || varName == "subbasin_downstream_id") {
            continue;
        }
        vecInt dimId = file.GetVarDimIds(iVar, 1);
        if (dimId[0] != dimIdSubbasins) {
            continue;
        }

        vecDouble values = file.GetVarDouble1D(varName, subbasinCount);
        string unit = file.HasAtt("units", varName) ? file.GetAttText("units", varName) : "";

        for (int i = 0; i < subbasinCount; ++i) {
            HydroUnitPropertyDouble prop;
            prop.name = varName;
            prop.value = values[i];
            prop.unit = unit;
            _subbasins[i].propertiesDouble.push_back(prop);
        }
    }
}

double SettingsBasin::GetTotalArea() const {
    double sum = 0;
    for (const auto& unit : _hydroUnits) {
        sum += unit.area;
    }

    return sum;
}
