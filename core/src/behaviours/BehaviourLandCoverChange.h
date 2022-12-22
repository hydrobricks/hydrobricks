#ifndef HYDROBRICKS_BEHAVIOUR_LAND_COVER_CHANGE_H
#define HYDROBRICKS_BEHAVIOUR_LAND_COVER_CHANGE_H

#include "Behaviour.h"
#include "Includes.h"

class BehaviourLandCoverChange : public Behaviour {
  public:
    BehaviourLandCoverChange();

    ~BehaviourLandCoverChange() override = default;

    void AddChange(double date, int hydroUnitId, const string& landCoverName, double area);

    bool Apply(double date) override;

  protected:
    vecInt m_hydroUnitIds;
    vecInt m_landCoverIds;
    vecStr m_landCoverNames;
    vecDouble m_areas;

  private:
    int GetLandCoverId(const string& landCoverName);
};

#endif  // HYDROBRICKS_BEHAVIOUR_LAND_COVER_CHANGE_H
