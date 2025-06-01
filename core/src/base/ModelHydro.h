#ifndef HYDROBRICKS_MODEL_HYDRO_H
#define HYDROBRICKS_MODEL_HYDRO_H

#include "ActionsManager.h"
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

    /**
     * Initialize the model along with the basin.
     *
     * @param modelSettings settings of the model.
     * @param basinSettings settings of the basin.
     * @return true if the initialization was successful.
     */
    bool InitializeWithBasin(SettingsModel& modelSettings, SettingsBasin& basinSettings);

    /**
     * Initialize the model.
     *
     * @param modelSettings settings of the model.
     * @param basinSettings settings of the basin.
     * @return true if the initialization was successful.
     */
    bool Initialize(SettingsModel& modelSettings, SettingsBasin& basinSettings);

    /**
     * Update the model parameters.
     *
     * @param modelSettings settings of the model.
     */
    void UpdateParameters(SettingsModel& modelSettings);

    /**
     * Check if the model is well-defined.
     *
     * @return true if the model is well-defined.
     */
    bool IsOk();

    /**
     * Check if the forcing data were loaded.
     *
     * @return true if the forcing data were loaded.
     */
    bool ForcingLoaded();

    /**
     * Run the model.
     *
     * @return true if the model run was successful.
     */
    bool Run();

    /**
     * Reset the model.
     */
    void Reset();

    /**
     * Save the model state as initial conditions.
     */
    void SaveAsInitialState();

    /**
     * Dump the outputs as betCDF file to the specified path.
     *
     * @param path path to dump the outputs.
     * @return true if the dump was successful.
     */
    bool DumpOutputs(const string& path);

    /**
     * Get the outlet discharge series.
     *
     * @return outlet discharge.
     */
    axd GetOutletDischarge();

    /**
     * Get the total outlet discharge.
     *
     * @return total outlet discharge.
     */
    double GetTotalOutletDischarge();

    /**
     * Get the total amount of water lost by evapotranspiration.
     *
     * @return total amount of water lost by evapotranspiration.
     */
    double GetTotalET();

    /**
     * Get the total change in water storage.
     *
     * @return total change in water storage.
     */
    double GetTotalWaterStorageChanges();

    /**
     * Get the total change in snow storage.
     *
     * @return total change in snow storage.
     */
    double GetTotalSnowStorageChanges();

    /*
     * Get the total change in glacier storage.
     *
     * @return total change in glacier storage.
     */
    double GetTotalGlacierStorageChanges();

    /**
     * Add a time series to the model.
     *
     * @param timeSeries time series to add.
     * @return true if the time series was added successfully.
     */
    bool AddTimeSeries(TimeSeries* timeSeries);

    /**
     * Add an action to the model.
     *
     * @param action action to add.
     * @return true if the action was added successfully.
     */
    bool AddAction(Action* action);

    /**
     * Get the number of actions in the model.
     *
     * @return number of actions.
     */
    int GetActionsNb();

    /**
     * Get the number of sporadic action items in the model (i.e., actions that are not recursive).
     *
     * @return number of sporadic action items.
     */
    int GetSporadicActionItemsNb();

    /**
     * Create a time series and add it to the model.
     *
     * @param varName name of the variable.
     * @param time time series data.
     * @param ids ids of the data.
     * @param data data to add.
     * @return true if the time series was created and added successfully.
     */
    bool CreateTimeSeries(const string& varName, const axd& time, const axi& ids, const axxd& data);

    /**
     * Clear the time series.
     */
    void ClearTimeSeries();

    /**
     * Attach the time series to the hydro units.
     *
     * @return true if the time series were attached successfully.
     */
    bool AttachTimeSeriesToHydroUnits();

    /**
     * Get the sub basin.
     *
     * @return pointer to the sub basin.
     */
    SubBasin* GetSubBasin() {
        return m_subBasin;
    }

    /**
     * Set the sub basin.
     *
     * @param subBasin pointer to the sub basin.
     */
    void SetSubBasin(SubBasin* subBasin) {
        m_subBasin = subBasin;
    }

    /**
     * Get the time machine (timer).
     *
     * @return pointer to the timer.
     */
    TimeMachine* GetTimeMachine() {
        return &m_timer;
    }

    /**
     * Get the processor.
     *
     * @return pointer to the processor.
     */
    Processor* GetProcessor() {
        return &m_processor;
    }

    /**
     * Get the logger.
     *
     * @return pointer to the logger.
     */
    Logger* GetLogger() {
        return &m_logger;
    }

    /**
     * Get the actions manager.
     *
     * @return pointer to the actions manager.
     */
    ActionsManager* GetActionsManager() {
        return &m_actionsManager;
    }

  protected:
    Processor m_processor;
    SubBasin* m_subBasin;
    TimeMachine m_timer;
    Logger m_logger;
    ActionsManager m_actionsManager;
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
