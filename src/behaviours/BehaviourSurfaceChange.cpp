#include "BehaviourSurfaceChange.h"
#include "FileNetcdf.h"

BehaviourSurfaceChange::BehaviourSurfaceChange() {}

void BehaviourSurfaceChange::AddChange(double date, int hydroUnitId, int surfaceTypeId, double area) {
    m_dates.push_back(date);
    m_hydroUnitIds.push_back(hydroUnitId);
    m_surfaceTypeIds.push_back(surfaceTypeId);
    m_area.push_back(area);
}

bool BehaviourSurfaceChange::Parse(const wxString &path) {

    try {
        FileNetcdf file;

        if (!file.OpenReadOnly(path)) {
            return false;
        }

        // Get number of changes
        int changesNb = file.GetDimLen("changes");

        // Get ids
        m_hydroUnitIds = file.GetVarInt1D("hydro_unit", changesNb);

        // Get areas
        m_area = file.GetVarDouble1D("area", changesNb);

        // Get time
        m_dates = file.GetVarDouble1D("time", changesNb);

        // Get surface types
        m_surfaceTypeIds = file.GetVarInt1D("surface_type", changesNb);

        // Get the surface names
        vecStr surfaces = file.GetAttString1D("surface_names");




        // Get surface data
        for (const auto& surface: surfaces) {
            vecFloat fractions = file.GetVarFloat1D(surface, changesNb);

            // Get the surface type
            wxString type = file.GetAttText("type", surface);

        }


    } catch(std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}