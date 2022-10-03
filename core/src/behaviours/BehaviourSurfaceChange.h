#ifndef HYDROBRICKS_BEHAVIOUR_SURFACE_CHANGE_H
#define HYDROBRICKS_BEHAVIOUR_SURFACE_CHANGE_H

#include "Behaviour.h"
#include "Includes.h"

class BehaviourSurfaceChange : public Behaviour {
  public:
    BehaviourSurfaceChange();

    ~BehaviourSurfaceChange() override = default;

    void AddChange(double date, int hydroUnitId, int surfaceTypeId, double area);

    bool Parse(const std::string& path);

  protected:
    vecDouble m_dates;
    vecInt m_hydroUnitIds;
    vecInt m_surfaceTypeIds;
    vecDouble m_area;

  private:
};

#endif  // HYDROBRICKS_BEHAVIOUR_SURFACE_CHANGE_H
