#ifndef HYDROBRICKS_MODEL_HYDRO_H
#define HYDROBRICKS_MODEL_HYDRO_H

#include "Includes.h"
#include "Logger.h"
#include "Processor.h"
#include "SettingsModel.h"
#include "SubBasin.h"
#include "TimeSeries.h"

class ModelHydro : public wxObject {
  public:
    ModelHydro(SubBasin* subBasin);

    ~ModelHydro() override;

    bool Initialize(SettingsModel& modelSettings);

    bool IsOk();

    bool Run();

    bool AddTimeSeries(TimeSeries* timeSeries);

    bool AttachTimeSeriesToHydroUnits();

    SubBasin* GetSubBasin() {
        return m_subBasin;
    }

    TimeMachine* GetTimeMachine() {
        return &m_timer;
    }

    Processor* GetProcessor() {
        return &m_processor;
    }

    Logger* GetLogger() {
        return &m_logger;
    }

  protected:
    Processor m_processor;
    SubBasin* m_subBasin;
    TimeMachine m_timer;
    Logger m_logger;
    std::vector<TimeSeries*> m_timeSeries;

  private:
    void BuildModelStructure(SettingsModel& modelSettings);

    void LinkRelatedSurfaceBricks(SettingsModel& modelSettings, HydroUnit* unit);

    void LinkProcessesTargetBricks(SettingsModel& modelSettings, HydroUnit* unit);

    void BuildForcingConnections(BrickSettings& brickSettings, HydroUnit* unit, Brick* brick);

    void BuildForcingConnections(ProcessSettings &processSettings, HydroUnit* unit, Process* process);

    void BuildForcingConnections(SplitterSettings& splitterSettings, HydroUnit* unit, Splitter* splitter);

    void BuildBricksFluxes(SettingsModel& modelSettings, HydroUnit* unit);

    void BuildSplittersFluxes(SettingsModel& modelSettings, HydroUnit* unit);

    void ConnectLoggerToValues(SettingsModel& modelSettings);

    bool InitializeTimeSeries();

    bool UpdateForcing();
};

#endif  // HYDROBRICKS_MODEL_HYDRO_H
