#ifndef HYDROBRICKS_MODEL_HYDRO_H
#define HYDROBRICKS_MODEL_HYDRO_H

#include "BehavioursManager.h"
#include "Includes.h"
#include "Logger.h"
#include "Processor.h"
#include "SettingsModel.h"
#include "SubBasin.h"
#include "TimeSeries.h"

class ModelHydro : public wxObject {
  public:
    ModelHydro(SubBasin* subBasin = nullptr);

    ~ModelHydro() override;

    bool InitializeWithBasin(SettingsModel& modelSettings, SettingsBasin& basinSettings);

    bool Initialize(SettingsModel& modelSettings, SettingsBasin& basinProp);

    void UpdateParameters(SettingsModel& modelSettings);

    bool IsOk();

    bool ForcingLoaded();

    bool Run();

    void Reset();

    void SaveAsInitialState();

    bool DumpOutputs(const string& path);

    axd GetOutletDischarge();

    double GetTotalOutletDischarge();

    double GetTotalET();

    double GetTotalWaterStorageChanges();

    double GetTotalSnowStorageChanges();

    bool AddTimeSeries(TimeSeries* timeSeries);

    bool AddBehaviour(Behaviour* behaviour);

    int GetBehavioursNb();

    int GetBehaviourItemsNb();

    bool CreateTimeSeries(const string& varName, const axd& time, const axi& ids, const axxd& data);

    void ClearTimeSeries();

    bool AttachTimeSeriesToHydroUnits();

    SubBasin* GetSubBasin() {
        return m_subBasin;
    }

    void SetSubBasin(SubBasin* subBasin) {
        m_subBasin = subBasin;
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

    BehavioursManager* GetBehavioursManager() {
        return &m_behavioursManager;
    }

  protected:
    Processor m_processor;
    SubBasin* m_subBasin;
    TimeMachine m_timer;
    Logger m_logger;
    BehavioursManager m_behavioursManager;
    ParametersUpdater m_parametersUpdater;
    vector<TimeSeries*> m_timeSeries;

  private:
    void BuildModelStructure(SettingsModel& modelSettings);

    void CreateSubBasinComponents(SettingsModel& modelSettings);

    void CreateHydroUnitsComponents(SettingsModel& modelSettings);

    void CreateHydroUnitBrick(SettingsModel& modelSettings, HydroUnit* unit, int iBrick);

    void UpdateSubBasinParameters(SettingsModel& modelSettings);

    void UpdateHydroUnitsParameters(SettingsModel& modelSettings);

    void LinkSurfaceComponentsParents(SettingsModel& modelSettings, HydroUnit* unit);

    void LinkSubBasinProcessesTargetBricks(SettingsModel& modelSettings);

    void LinkHydroUnitProcessesTargetBricks(SettingsModel& modelSettings, HydroUnit* unit);

    void BuildForcingConnections(BrickSettings& brickSettings, HydroUnit* unit, Brick* brick);

    void BuildForcingConnections(ProcessSettings& processSettings, HydroUnit* unit, Process* process);

    void BuildForcingConnections(SplitterSettings& splitterSettings, HydroUnit* unit, Splitter* splitter);

    void BuildSubBasinBricksFluxes(SettingsModel& modelSettings);

    void BuildHydroUnitBricksFluxes(SettingsModel& modelSettings, HydroUnit* unit);

    void BuildSubBasinSplittersFluxes(SettingsModel& modelSettings);

    void BuildHydroUnitSplittersFluxes(SettingsModel& modelSettings, HydroUnit* unit);

    void ConnectLoggerToValues(SettingsModel& modelSettings);

    bool InitializeTimeSeries();

    bool UpdateForcing();
};

#endif  // HYDROBRICKS_MODEL_HYDRO_H
