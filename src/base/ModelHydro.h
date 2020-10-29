
#ifndef FLHY_MODEL_HYDRO_H
#define FLHY_MODEL_HYDRO_H

#include "Includes.h"
#include "SubBasin.h"
#include "TimeSeries.h"

class ModelHydro : public wxObject {
  public:
    ModelHydro(SubBasin* subBasin, const TimeMachine& timer);

    ~ModelHydro() override = default;

    bool IsOk();

    void StartModelling();

    void SetTimeSeries(TimeSeries* timeSeries);

  protected:
    SubBasin* m_subBasin;
    TimeMachine m_timer;
    TimeSeries* m_timeSeries[MAX_VAR_TYPES];

  private:
};

#endif  // FLHY_MODEL_HYDRO_H
