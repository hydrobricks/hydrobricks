#ifndef HYDROBRICKS_BEHAVIOUR_LAND_COVER_CHANGE_H
#define HYDROBRICKS_BEHAVIOUR_LAND_COVER_CHANGE_H

#include "Behaviour.h"
#include "Includes.h"

class BehaviourLandCoverChange : public Behaviour {
  public:
    BehaviourLandCoverChange();

    ~BehaviourLandCoverChange() override = default;

    void AddChange(double date, int hydroUnitId, int landCoverTypeId, double area);

    bool Parse(const std::string& path);

  protected:
    vecDouble m_dates;
    vecInt m_hydroUnitIds;
    vecInt m_landCoverTypeIds;
    vecDouble m_area;

  private:
};

#endif  // HYDROBRICKS_BEHAVIOUR_LAND_COVER_CHANGE_H
