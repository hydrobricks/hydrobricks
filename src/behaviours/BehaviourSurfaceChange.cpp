#include "BehaviourSurfaceChange.h"

BehaviourSurfaceChange::BehaviourSurfaceChange() {}

void BehaviourSurfaceChange::AddChange(double date, int hydroUnitId, int surfaceTypeId, double area) {
    m_dates.push_back(date);
    m_hydroUnitIds.push_back(hydroUnitId);
    m_surfaceTypeIds.push_back(surfaceTypeId);
    m_area.push_back(area);
}

bool BehaviourSurfaceChange::Parse(const wxString &path) {

}