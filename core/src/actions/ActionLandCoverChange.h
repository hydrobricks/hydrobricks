#ifndef HYDROBRICKS_ACTION_LAND_COVER_CHANGE_H
#define HYDROBRICKS_ACTION_LAND_COVER_CHANGE_H

#include "Action.h"
#include "Includes.h"

class ActionLandCoverChange : public Action {
  public:
    ActionLandCoverChange();

    ~ActionLandCoverChange() override = default;

    void AddChange(double date, int hydroUnitId, const string& landCoverName, double area);

    bool Apply(double date) override;

    int GetChangesNb() {
        return int(m_dates.size());
    }

    int GetLandCoversNb() {
        return int(m_landCoverNames.size());
    }

  protected:
    vecInt m_hydroUnitIds;
    vecInt m_landCoverIds;
    vecStr m_landCoverNames;
    vecDouble m_areas;

  private:
    int GetLandCoverId(const string& landCoverName);
};

#endif  // HYDROBRICKS_ACTION_LAND_COVER_CHANGE_H
