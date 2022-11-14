#include "BehaviourLandCoverChange.h"

#include "FileNetcdf.h"

BehaviourLandCoverChange::BehaviourLandCoverChange() {}

void BehaviourLandCoverChange::AddChange(double date, int hydroUnitId, int landCoverTypeId, double area) {
    m_dates.push_back(date);
    m_hydroUnitIds.push_back(hydroUnitId);
    m_landCoverTypeIds.push_back(landCoverTypeId);
    m_area.push_back(area);
}

bool BehaviourLandCoverChange::Parse(const std::string& path) {
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
            vecFloat fractions = file.GetVarFloat1D(landCover, changesNb);

            // Get the land cover type
            std::string type = file.GetAttText("type", landCover);
        }

    } catch (std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}
