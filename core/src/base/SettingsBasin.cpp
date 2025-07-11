#include "SettingsBasin.h"

#include "FileNetcdf.h"
#include "Parameter.h"

SettingsBasin::SettingsBasin()
    : _selectedHydroUnit(nullptr) {}

SettingsBasin::~SettingsBasin() = default;

void SettingsBasin::AddHydroUnit(int id, double area, double elevation) {
    HydroUnitSettings unit;
    unit.id = id;
    unit.area = area;
    unit.elevation = elevation;
    _hydroUnits.push_back(unit);
    _selectedHydroUnit = &_hydroUnits[_hydroUnits.size() - 1];
}

void SettingsBasin::AddLandCover(const string& name, const string& type, double fraction) {
    wxASSERT(_selectedHydroUnit);

    LandCoverSettings element;
    element.name = name;
    element.type = type;
    element.fraction = fraction;
    _selectedHydroUnit->landCovers.push_back(element);
}

void SettingsBasin::AddHydroUnitPropertyDouble(const string& name, double value, const string& unit) {
    wxASSERT(_selectedHydroUnit);

    HydroUnitPropertyDouble property;
    property.name = name;
    property.value = value;
    property.unit = unit;
    _selectedHydroUnit->propertiesDouble.push_back(property);
}

void SettingsBasin::AddHydroUnitPropertyString(const string& name, const string& value) {
    wxASSERT(_selectedHydroUnit);

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
}

void SettingsBasin::SelectUnit(int index) {
    wxASSERT(_hydroUnits.size() > index);
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
        int unitsNb = file.GetDimLen("hydro_units");

        // Get ids
        vecInt ids = file.GetVarInt1D("id", unitsNb);

        // Get areas
        vecDouble areas = file.GetVarDouble1D("area", unitsNb);

        // Store hydro units
        for (int iUnit = 0; iUnit < unitsNb; ++iUnit) {
            HydroUnitSettings unit;
            unit.id = ids[iUnit];
            unit.area = areas[iUnit];
            _hydroUnits.push_back(unit);
        }

        // Get land cover data
        for (const auto& cover : landCovers) {
            vecDouble fractions = file.GetVarDouble1D(cover, unitsNb);

            // Get the cover type
            string type = file.GetAttText("type", cover);

            for (int iUnit = 0; iUnit < unitsNb; ++iUnit) {
                LandCoverSettings element;
                element.name = cover;
                element.type = type;
                element.fraction = fractions[iUnit];
                _hydroUnits[iUnit].landCovers.push_back(element);
            }
        }

        // Extract other variables
        int varsNb = file.GetVarsNb();
        int dimIdHydroUnits = file.GetDimId("hydro_units");

        for (int iVar = 0; iVar < varsNb; ++iVar) {
            // Get variable name
            string varName = file.GetVarName(iVar);

            if (varName == "id" || varName == "area") {
                continue;
            }

            vecInt dimId = file.GetVarDimIds(iVar, 1);
            if (dimId[0] != dimIdHydroUnits) {
                continue;
            }

            vecDouble values = file.GetVarDouble1D(varName, unitsNb);

            for (int iUnit = 0; iUnit < unitsNb; ++iUnit) {
                HydroUnitPropertyDouble prop;
                prop.name = varName;
                prop.value = values[iUnit];
                prop.unit = "";
                _hydroUnits[iUnit].propertiesDouble.push_back(prop);
            }
        }

    } catch (std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}

double SettingsBasin::GetTotalArea() const {
    double sum = 0;
    for (const auto& unit : _hydroUnits) {
        sum += unit.area;
    }

    return sum;
}