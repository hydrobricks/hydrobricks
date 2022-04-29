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

    bool Run();

    bool AddTimeSeries(TimeSeries* timeSeries);

    bool AttachTimeSeriesToHydroUnits();

    SubBasin* GetSubBasin() {
        return m_subBasin;
    }

  protected:
    Processor* m_processor;
    SubBasin* m_subBasin;
    TimeMachine* m_timer;
    std::vector<TimeSeries*> m_timeSeries;

  private:
    static void BuildModelStructure(ParameterSet &parameterSet, SubBasin* subBasin);

    static void BuildForcingConnections(BrickSettings& brickSettings, HydroUnit* unit, Brick* brick);

    static void BuildFluxes(const ParameterSet& parameterSet, SubBasin* subBasin, HydroUnit* unit);

    bool InitializeTimeSeries();

    bool UpdateForcing();
};

#endif  // HYDROBRICKS_MODEL_HYDRO_H
