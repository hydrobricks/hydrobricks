
#ifndef FLHY_MODEL_HYDRO_H
#define FLHY_MODEL_HYDRO_H

#include "Includes.h"
#include "SubCatchment.h"
#include "TimeSeries.h"

class ModelHydro : public wxObject {
  public:
    ModelHydro(SubCatchment* subCatchment, const TimeMachine& timer);

    ~ModelHydro() override = default;

    void StartModelling();

    void SetTimeSeries(TimeSeries* timeSeries);

  protected:
    SubCatchment* m_subCatchment;
    TimeMachine m_timer;
    TimeSeries* m_timeSeries[MAX_VAR_TYPES];

  private:
};

#endif  // FLHY_MODEL_HYDRO_H
