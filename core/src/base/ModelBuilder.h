#ifndef HYDROBRICKS_MODEL_BUILDER_H
#define HYDROBRICKS_MODEL_BUILDER_H

#include "Includes.h"

class SettingsModel;
class SettingsBasin;
class SubBasin;
class HydroUnit;
class Brick;
class Process;
class Splitter;
class TimeMachine;
class Logger;
struct BrickSettings;
struct ProcessSettings;
struct SplitterSettings;

class ModelBuilder {
  public:
    ModelBuilder(SubBasin* subBasin, TimeMachine* timer, Logger* logger);

    /**
     * Assign each hydro unit the structure variant whose land-cover set matches the
     * unit's present land covers (fraction > 0). No-op when a single structure is
     * defined. Must run before BuildModelStructure so units build their variant.
     *
     * @param modelSettings the model settings (defines the structure variants).
     * @param basinSettings the basin settings (per-unit land covers and fractions).
     */
    void AssignHydroUnitStructures(SettingsModel& modelSettings, SettingsBasin& basinSettings);

    /**
     * Build the full model structure. The sub-basin components are built from the
     * primary structure (1), then each hydro unit builds the structure variant it was
     * assigned by AssignHydroUnitStructures.
     *
     * @param modelSettings the model settings (defines the structure variants).
     */
    void BuildModelStructure(SettingsModel& modelSettings);

    /**
     * Update the parameters of the sub-basin bricks, their processes and the sub-basin
     * splitters from the (primary) structure settings, without rebuilding the model.
     *
     * @param modelSettings the model settings holding the updated parameter values.
     */
    void UpdateSubBasinParameters(SettingsModel& modelSettings);

    /**
     * Update the parameters of every hydro unit's bricks, processes and splitters
     * without rebuilding the model. Each unit's assigned structure variant is selected
     * so the updated values match the components that unit was built with.
     *
     * @param modelSettings the model settings holding the updated parameter values.
     */
    void UpdateHydroUnitsParameters(SettingsModel& modelSettings);

    /**
     * Connect the logger entries to the corresponding value pointers in the sub-basin
     * and hydro unit components, so logged items are recorded during the simulation.
     *
     * @param modelSettings the model settings (defines the items to log).
     */
    void ConnectLoggerToValues(SettingsModel& modelSettings);

  private:
    SubBasin* _subBasin;
    TimeMachine* _timer;
    Logger* _logger;

    void CreateSubBasinComponents(SettingsModel& modelSettings);
    void CreateHydroUnitsComponents(SettingsModel& modelSettings);
    void CreateHydroUnitBrick(SettingsModel& modelSettings, HydroUnit* unit, int iBrick);
    void PopulateSpatialOverrides(SettingsModel& modelSettings, HydroUnit* unit, const string& brickName);
    void LinkSurfaceComponentsParents(SettingsModel& modelSettings, HydroUnit* unit);
    void LinkSubBasinProcessesTargetBricks(SettingsModel& modelSettings);
    void LinkHydroUnitProcessesTargetBricks(SettingsModel& modelSettings, HydroUnit* unit);
    std::vector<Brick*> FindLandCoversFeeding(const string& targetName, HydroUnit* unit, SettingsModel& modelSettings);
    void BuildForcingConnections(const BrickSettings& brickSettings, HydroUnit* unit, Brick* brick);
    void BuildForcingConnections(const ProcessSettings& processSettings, HydroUnit* unit, Process* process);
    void BuildForcingConnections(const SplitterSettings& splitterSettings, HydroUnit* unit, Splitter* splitter);
    void BuildSubBasinBricksFluxes(SettingsModel& modelSettings);
    void BuildHydroUnitBricksFluxes(SettingsModel& modelSettings, HydroUnit* unit);
    void BuildSubBasinSplittersFluxes(SettingsModel& modelSettings);
    void BuildHydroUnitSplittersFluxes(SettingsModel& modelSettings, HydroUnit* unit);
};

#endif  // HYDROBRICKS_MODEL_BUILDER_H
