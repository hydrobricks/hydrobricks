#include "BehaviourLandCoverChange.h"

#include "FileNetcdf.h"
#include "HydroUnit.h"
#include "ModelHydro.h"

BehaviourLandCoverChange::BehaviourLandCoverChange() = default;

void BehaviourLandCoverChange::AddChange(double date, int hydroUnitId, const string& landCoverName, double area) {
    int landCoverId = GetLandCoverId(landCoverName);

    m_dates.push_back(date);
    m_hydroUnitIds.push_back(hydroUnitId);
    m_landCoverIds.push_back(landCoverId);
    m_areas.push_back(area);
}

bool BehaviourLandCoverChange::Apply(double) {
    wxASSERT(m_dates.size() > m_cursor);
    wxASSERT(m_hydroUnitIds.size() > m_cursor);
    wxASSERT(m_areas.size() > m_cursor);
    wxASSERT(m_landCoverIds.size() > m_cursor);
    wxASSERT(m_landCoverNames.size() > m_landCoverIds[m_cursor]);

    HydroUnit* unit = m_manager->GetHydroUnitById(m_hydroUnitIds[m_cursor]);
    string landCoverName = m_landCoverNames[m_landCoverIds[m_cursor]];
    double areaFraction = m_areas[m_cursor] / unit->GetArea();
    unit->ChangeLandCoverAreaFraction(landCoverName, areaFraction);

    return true;
}

int BehaviourLandCoverChange::GetLandCoverId(const string& landCoverName) {
    int landCoverId = -1;
    for (int i = 0; i < m_landCoverNames.size(); ++i) {
        if (m_landCoverNames[i] == landCoverName) {
            landCoverId = i;
            break;
        }
    }

    // Insert as a new land cover if not found.
    if (landCoverId == -1) {
        m_landCoverNames.push_back(landCoverName);
        landCoverId = m_landCoverNames.size() - 1;
    }

    return landCoverId;
}