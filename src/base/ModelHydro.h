#ifndef HYDROBRICKS_MODEL_HYDRO_H
#define HYDROBRICKS_MODEL_HYDRO_H

#include "Includes.h"
#include "ParameterSet.h"
#include "Processor.h"
#include "SubBasin.h"
#include "TimeSeries.h"

class ModelHydro : public wxObject {
  public:
    ModelHydro(Processor* processor, SubBasin* subBasin, TimeMachine* timer);

    ~ModelHydro() override;

    static ModelHydro* Factory(ParameterSet &parameterSet, SubBasin* subBasin);

    bool IsOk();

    bool UpdateForcing();

    bool Run();

    void SetTimeSeries(TimeSeries* timeSeries);

    SubBasin* GetSubBasin() {
        return m_subBasin;
    }

  protected:
    Processor* m_processor;
    SubBasin* m_subBasin;
    TimeMachine* m_timer;
    TimeSeries* m_timeSeries[MAX_VAR_TYPES];

  private:
    static void BuildModelStructure(ParameterSet &parameterSet, SubBasin* subBasin);

    static void BuildForcingConnections(BrickSettings& brickSettings, HydroUnit* unit, Brick* brick);

    static void BuildFluxes(const ParameterSet& parameterSet, SubBasin* subBasin, HydroUnit* unit);
};

#endif  // HYDROBRICKS_MODEL_HYDRO_H
