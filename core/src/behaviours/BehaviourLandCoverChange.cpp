#include "BehaviourLandCoverChange.h"

#include "FileNetcdf.h"
#include "HydroUnit.h"
#include "ModelHydro.h"

BehaviourLandCoverChange::BehaviourLandCoverChange() = default;

void BehaviourLandCoverChange::AddChange(double date, int hydroUnitId, const string& landCoverName, double area) {
    int landCoverId = GetLandCoverId(landCoverName);

    int index = GetIndexForInsertion(date);

    m_dates.insert(m_dates.begin() + index, date);
    m_hydroUnitIds.insert(m_hydroUnitIds.begin() + index, hydroUnitId);
    m_landCoverIds.insert(m_landCoverIds.begin() + index, landCoverId);
    m_areas.insert(m_areas.begin() + index, area);
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

    return unit->ChangeLandCoverAreaFraction(landCoverName, areaFraction);
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
        landCoverId = int(m_landCoverNames.size()) - 1;
    }

    return landCoverId;
}