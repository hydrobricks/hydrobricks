#include "BehaviourLandCoverChange.h"

#include "FileNetcdf.h"
#include "HydroUnit.h"
#include "ModelHydro.h"

BehaviourLandCoverChange::BehaviourLandCoverChange() {}

void BehaviourLandCoverChange::AddChange(double date, int hydroUnitId, int landCoverTypeId, double area) {
    m_dates.push_back(date);
    m_hydroUnitIds.push_back(hydroUnitId);
    m_landCoverTypeIds.push_back(landCoverTypeId);
    m_area.push_back(area);
}

bool BehaviourLandCoverChange::Parse(const string& path) {
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

        // Get land cover types
        m_landCoverTypeIds = file.GetVarInt1D("land_cover_type", changesNb);

        // Get the land cover names
        vecStr landCovers = file.GetAttString1D("land_cover_names");

        // Get land cover data
        for (const auto& landCover : landCovers) {
            vecDouble fractions = file.GetVarDouble1D(landCover, changesNb);

            // Get the land cover type
            string type = file.GetAttText("type", landCover);
        }

    } catch (std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}

bool BehaviourLandCoverChange::Apply(double date) {
    wxASSERT(m_dates.size() > m_cursor);

    HydroUnit* unit = m_manager->GetModel()->GetSubBasin()->GetHydroUnitById(m_hydroUnitIds[m_cursor]);

    return false;
}
