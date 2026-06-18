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

    void BuildModelStructure(SettingsModel& modelSettings);
    void UpdateSubBasinParameters(SettingsModel& modelSettings);
    void UpdateHydroUnitsParameters(SettingsModel& modelSettings);
    void ConnectLoggerToValues(SettingsModel& modelSettings);

  private:
    SubBasin* _subBasin;
    TimeMachine* _timer;
    Logger* _logger;

    void CreateSubBasinComponents(SettingsModel& modelSettings);
    void CreateHydroUnitsComponents(SettingsModel& modelSettings);
    void CreateHydroUnitBrick(SettingsModel& modelSettings, HydroUnit* unit, int iBrick);
    void LinkSurfaceComponentsParents(SettingsModel& modelSettings, HydroUnit* unit);
    void LinkSubBasinProcessesTargetBricks(SettingsModel& modelSettings);
    void LinkHydroUnitProcessesTargetBricks(SettingsModel& modelSettings, HydroUnit* unit);
    void BuildForcingConnections(const BrickSettings& brickSettings, HydroUnit* unit, Brick* brick);
    void BuildForcingConnections(const ProcessSettings& processSettings, HydroUnit* unit, Process* process);
    void BuildForcingConnections(const SplitterSettings& splitterSettings, HydroUnit* unit, Splitter* splitter);
    void BuildSubBasinBricksFluxes(SettingsModel& modelSettings);
    void BuildHydroUnitBricksFluxes(SettingsModel& modelSettings, HydroUnit* unit);
    void BuildSubBasinSplittersFluxes(SettingsModel& modelSettings);
    void BuildHydroUnitSplittersFluxes(SettingsModel& modelSettings, HydroUnit* unit);
};

#endif  // HYDROBRICKS_MODEL_BUILDER_H
