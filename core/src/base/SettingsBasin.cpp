#include "Parameter.h"
#include "SettingsBasin.h"
#include "FileNetcdf.h"


SettingsBasin::SettingsBasin()
{
}

SettingsBasin::~SettingsBasin() {
}

void SettingsBasin::AddHydroUnit(int id, double area, double elevation) {
    HydroUnitSettings unit;
    unit.id = id;
    unit.area = area;
    unit.elevation = elevation;
    m_hydroUnits.push_back(unit);
    m_selectedHydroUnit = &m_hydroUnits[m_hydroUnits.size() - 1];
}

void SettingsBasin::AddHydroUnitSurfaceElement(const std::string& name, double fraction) {
    wxASSERT(m_selectedHydroUnit);
    SurfaceElementSettings element;
    element.name = name;
    element.fraction = fraction;
    m_selectedHydroUnit->surfaceElements.push_back(element);
}

void SettingsBasin::SelectUnit(int index) {
    wxASSERT(m_hydroUnits.size() > index);
    m_selectedHydroUnit = &m_hydroUnits[index];
}

bool SettingsBasin::Parse(const std::string &path) {

    try {
        FileNetcdf file;

        if (!file.OpenReadOnly(path)) {
            return false;
        }

        // Get the surface names
        vecStr surfaces = file.GetAttString1D("surface_names");

        // Get number of units
        int unitsNb = file.GetDimLen("hydro_units");

        // Get ids
        vecInt ids = file.GetVarInt1D("id", unitsNb);

        // Get areas
        vecFloat areas = file.GetVarFloat1D("area", unitsNb);

        // Get elevations
        vecFloat elevations = file.GetVarFloat1D("elevation", unitsNb);

        // Store hydro units
        for (int iUnit = 0; iUnit < unitsNb; ++iUnit) {
            HydroUnitSettings unit;
            unit.id = ids[iUnit];
            unit.area = areas[iUnit];
            unit.elevation = elevations[iUnit];
            m_hydroUnits.push_back(unit);
        }

        // Get surface data
        for (const auto& surface: surfaces) {
            vecFloat fractions = file.GetVarFloat1D(surface, unitsNb);

            // Get the surface type
            std::string type = file.GetAttText("type", surface);

            for (int iUnit = 0; iUnit < unitsNb; ++iUnit) {
                SurfaceElementSettings element;
                element.name = surface;
                element.type = type;
                element.fraction = fractions[iUnit];
                m_hydroUnits[iUnit].surfaceElements.push_back(element);
            }
        }

    } catch(std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}
