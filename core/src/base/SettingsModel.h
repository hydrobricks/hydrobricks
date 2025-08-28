#ifndef HYDROBRICKS_SETTINGS_MODEL_H
#define HYDROBRICKS_SETTINGS_MODEL_H

#include <yaml-cpp/yaml.h>

#include "Includes.h"
#include "Parameter.h"

struct SolverSettings {
    string name;
};

struct TimerSettings {
    string start;
    string end;
    int timeStep = 1;
    string timeStepUnit;
};

struct OutputSettings {
    string target;
    string fluxType = "water";
    bool isInstantaneous = false;
    bool isStatic = false;
};

struct ProcessSettings {
    string name;
    string type;
    vecStr logItems;
    vector<Parameter*> parameters;
    vector<VariableType> forcing;
    vector<OutputSettings> outputs;
};

struct SplitterSettings {
    string name;
    string type;
    vecStr logItems;
    vector<Parameter*> parameters;
    vector<VariableType> forcing;
    vector<OutputSettings> outputs;
};

struct BrickSettings {
    string name;
    string type;
    string parent;
    vecStr logItems;
    vector<Parameter*> parameters;
    vector<VariableType> forcing;
    vector<ProcessSettings> processes;
};

struct ModelStructure {
    int id;
    vecStr logItems;
    vector<BrickSettings> hydroUnitBricks;
    vector<BrickSettings> subBasinBricks;
    vecInt landCoverBricks;
    vecInt surfaceComponentBricks;
    vector<SplitterSettings> hydroUnitSplitters;
    vector<SplitterSettings> subBasinSplitters;
};

class SettingsModel : public wxObject {
  public:
    explicit SettingsModel();

    ~SettingsModel() override;

    /**
     * Set the solver name.
     *
     * @param solverName name of the solver.
     */
    void SetSolver(const string& solverName);

    /**
     * Set the timer settings.
     *
     * @param start start date.
     * @param end end date.
     * @param timeStep time step.
     * @param timeStepUnit time step unit.
     */
    void SetTimer(const string& start, const string& end, int timeStep, const string& timeStepUnit);

    /**
     * Add a hydro unit brick to the model (e.g. storage).
     *
     * @param name name of the hydro unit brick.
     * @param type type of the hydro unit brick.
     */
    void AddHydroUnitBrick(const string& name, const std::string& type = "storage");

    /**
     * Add a sub-basin brick to the model (e.g. lumped storage).
     *
     * @param name name of the sub-basin brick.
     * @param type type of the sub-basin brick.
     */
    void AddSubBasinBrick(const string& name, const std::string& type = "storage");

    /**
     * Add a land cover brick to the model (e.g. glacier).
     *
     * @param name name of the land cover brick.
     * @param type type of the land cover brick.
     */
    void AddLandCoverBrick(const string& name, const string& type);

    /**
     * Add a surface component brick to the model (e.g. snowpack).
     *
     * @param name name of the surface component brick.
     * @param type type of the surface component brick.
     */
    void AddSurfaceComponentBrick(const string& name, const string& type);

    /**
     * Set the parent of the selected surface component brick.
     *
     * @param name name of the parent brick.
     */
    void SetSurfaceComponentParent(const string& name);

    /**
     * Add a parameter to the selected brick.
     *
     * @param name name of the parameter.
     * @param value value of the parameter.
     * @param type type of the parameter.
     */
    void AddBrickParameter(const string& name, float value, const std::string& type = "constant");

    /**
     * Set the value of a parameter of the selected brick.
     *
     * @param name name of the parameter.
     * @param value value of the parameter.
     * @param type type of the parameter.
     */
    void SetBrickParameterValue(const string& name, float value, const std::string& type = "constant");

    /**
     * Check if the selected brick has a parameter.
     *
     * @param name name of the parameter.
     * @return true if the brick has the parameter, false otherwise.
     */
    bool BrickHasParameter(const string& name);

    /**
     * Add a forcing to the selected brick.
     *
     * @param name name of the forcing.
     */
    void AddBrickForcing(const string& name);

    /**
     * Add a process to the selected brick.
     *
     * @param name name of the process.
     * @param type type of the process.
     * @param target target of the process.
     * @param log true if the process should be logged, false otherwise.
     */
    void AddBrickProcess(const string& name, const string& type, const string& target = "", bool log = false);

    /**
     * Add a parameter to the selected process.
     *
     * @param name name of the parameter.
     * @param value value of the parameter.
     * @param type type of the parameter.
     */
    void AddProcessParameter(const string& name, float value, const std::string& type = "constant");

    /**
     * Set the value of the selected process parameter.
     *
     * @param name name of the parameter.
     * @param value value of the parameter.
     * @param type type of the parameter.
     */
    void SetProcessParameterValue(const string& name, float value, const std::string& type = "constant");

    /**
     * Add a forcing to the selected process.
     *
     * @param name name of the forcing.
     */
    void AddProcessForcing(const string& name);

    /**
     * Add an output to the selected process.
     *
     * @param target target of the output.
     * @param fluxType type of the flux.
     */
    void AddProcessOutput(const string& target, const string& fluxType = "water");

    /**
     * Set the outputs of the selected process as instantaneous.
     */
    void SetProcessOutputsAsInstantaneous();

    /**
     * Set the outputs of the selected process as static.
     */
    void SetProcessOutputsAsStatic();

    /**
     * Set the outputs of the selected process to the same brick.
     */
    void OutputProcessToSameBrick();

    /**
     * Add a hydro unit splitter to the selected structure.
     *
     * @param name name of the hydro unit splitter.
     * @param type type of the hydro unit splitter.
     */
    void AddHydroUnitSplitter(const string& name, const string& type);

    /**
     * Add a sub-basin splitter to the selected structure.
     *
     * @param name name of the sub-basin splitter.
     * @param type type of the sub-basin splitter.
     */
    void AddSubBasinSplitter(const string& name, const string& type);

    /**
     * Add a parameter to the selected splitter.
     *
     * @param name name of the parameter.
     * @param value value of the parameter.
     * @param type type of the parameter.
     */
    void AddSplitterParameter(const string& name, float value, const std::string& type = "constant");

    /**
     * Set the value of the selected splitter parameter.
     *
     * @param name name of the parameter.
     * @param value value of the parameter.
     * @param type type of the parameter.
     */
    void SetSplitterParameterValue(const string& name, float value, const std::string& type = "constant");

    /**
     * Add a forcing to the selected splitter.
     *
     * @param name name of the forcing.
     */
    void AddSplitterForcing(const string& name);

    /**
     * Add an output to the selected splitter.
     *
     * @param target target of the output.
     * @param fluxType type of the flux.
     */
    void AddSplitterOutput(const string& target, const string& fluxType = "water");

    /**
     * Add logging to a given item.
     *
     * @param itemName name of the item to log.
     */
    void AddLoggingToItem(const string& itemName);

    /**
     * Add logging to a list of items.
     *
     * @param items list of items to log.
     */
    void AddLoggingToItems(std::initializer_list<const string> items);

    /**
     * Add logging to the selected brick.
     *
     * @param itemName name of the item to log.
     */
    void AddBrickLogging(const string& itemName);

    /**
     * Add logging to a list of items in the selected brick.
     *
     * @param items list of items to log.
     */
    void AddBrickLogging(std::initializer_list<const string> items);

    /**
     * Add logging to the selected process.
     *
     * @param itemName name of the item to log.
     */
    void AddProcessLogging(const string& itemName);

    /**
     * Add logging to the selected splitter.
     *
     * @param itemName name of the item to log.
     */
    void AddSplitterLogging(const string& itemName);

    /**
     * Generate precipitation splitters (rain/snow).
     *
     * @param withSnow true if snow is included, false otherwise.
     */
    void GeneratePrecipitationSplitters(bool withSnow);

    /**
     * Generate snowpacks.
     *
     * @param snowMeltProcess name of the snow melt process.
     */
    void GenerateSnowpacks(const string& snowMeltProcess);

    /**
     * Add a snow-ice transformation process to the model.
     * This is used for glacier bricks to transform snow into ice.
     *
     * @param transformationProcess name of the transformation process.
     */
    void AddSnowIceTransformation(const string& transformationProcess = "transform:snow_ice_swat");

    /**
     * Add a snow redistribution process to the model.
     *
     * @param redistributionProcess name of the redistribution process.
     * @param skipGlaciers if true, do not redistribute snow for snowpacks on glaciers.
     */
    void AddSnowRedistribution(const string& redistributionProcess = "transport:snow_slide", bool skipGlaciers = false);

    /**
     * Generate snowpacks with water retention.
     *
     * @param snowMeltProcess name of the snow melt process.
     * @param outflowProcess name of the outflow process.
     */
    void GenerateSnowpacksWithWaterRetention(const string& snowMeltProcess, const string& outflowProcess);

    /**
     * Select a structure by its ID.
     *
     * @param id ID of the structure.
     * @return true if the structure is found, false otherwise.
     */
    bool SelectStructure(int id);

    /**
     * Select a hydro unit brick by its index.
     *
     * @param index index of the hydro unit brick.
     */
    void SelectHydroUnitBrick(int index);

    /**
     * Select a sub-basin brick by its index.
     *
     * @param index index of the sub-basin brick.
     */
    void SelectSubBasinBrick(int index);

    /**
     * Select a hydro unit brick by its name. Tolerate if the brick is not found.
     *
     * @param name name of the hydro unit brick.
     * @return true if the hydro unit brick is found, false otherwise.
     */
    bool SelectHydroUnitBrickIfFound(const string& name);

    /**
     * Select a sub-basin brick by its name. Tolerate if the brick is not found.
     *
     * @param name name of the sub-basin brick.
     * @return true if the sub-basin brick is found, false otherwise.
     */
    bool SelectSubBasinBrickIfFound(const string& name);

    /**
     * Select a hydro unit brick by its name. Raise an exception if the brick is not found.
     *
     * @param name name of the hydro unit brick.
     */
    void SelectHydroUnitBrick(const string& name);

    /**
     * Select a hydro unit brick by its name. Raise an exception if the brick is not found.
     * Same as SelectHydroUnitBrick, but used in the Python bindings.
     *
     * @param name name of the hydro unit brick.
     */
    void SelectHydroUnitBrickByName(const string& name);

    /**
     * Select a sub-basin brick by its name. Raise an exception if the brick is not found.
     *
     * @param name name of the sub-basin brick.
     */
    void SelectSubBasinBrick(const string& name);

    /**
     * Select a process by its index.
     *
     * @param index index of the process.
     */
    void SelectProcess(int index);

    /**
     * Select a process by its name.
     *
     * @param name name of the process.
     */
    void SelectProcess(const string& name);

    /**
     * Select a process by one of its parameter name.
     *
     * @param name name of the process.
     */
    void SelectProcessWithParameter(const string& name);

    /**
     * Select a hydro unit splitter by its index.
     *
     * @param index index of the hydro unit splitter.
     */
    void SelectHydroUnitSplitter(int index);

    /**
     * Select a sub-basin splitter by its index.
     *
     * @param index index of the sub-basin splitter.
     */
    void SelectSubBasinSplitter(int index);

    /**
     * Select a hydro unit splitter by its name. Tolerate if the splitter is not found.
     *
     * @param name name of the hydro unit splitter.
     * @return true if the hydro unit splitter is found, false otherwise.
     */
    bool SelectHydroUnitSplitterIfFound(const string& name);

    /**
     * Select a sub-basin splitter by its name. Tolerate if the splitter is not found.
     *
     * @param name name of the sub-basin splitter.
     * @return true if the sub-basin splitter is found, false otherwise.
     */
    bool SelectSubBasinSplitterIfFound(const string& name);

    /**
     * Select a hydro unit splitter by its name. Raise an exception if the splitter is not found.
     *
     * @param name name of the hydro unit splitter.
     */
    void SelectHydroUnitSplitter(const string& name);

    /**
     * Select a sub-basin splitter by its name. Raise an exception if the splitter is not found.
     *
     * @param name name of the sub-basin splitter.
     */
    void SelectSubBasinSplitter(const string& name);

    /**
     * Set parameter value for a component.
     *
     * @param component name of the component.
     * @param name name of the parameter.
     * @param value value of the parameter.
     * @return true if the parameter is set, false otherwise.
     */
    bool SetParameterValue(const string& component, const string& name, float value);

    /**
     * Get the number of structures in the model.
     *
     * @return number of structures.
     */
    int GetStructuresNb() const {
        return static_cast<int>(_modelStructures.size());
    }

    /**
     * Get the number of hydro unit bricks in the selected structure.
     *
     * @return number of hydro unit bricks.
     */
    int GetHydroUnitBricksNb() const {
        wxASSERT(_selectedStructure);
        return static_cast<int>(_selectedStructure->hydroUnitBricks.size());
    }

    /**
     * Get the number of sub-basin bricks in the selected structure.
     *
     * @return number of sub-basin bricks.
     */
    int GetSubBasinBricksNb() const {
        wxASSERT(_selectedStructure);
        return static_cast<int>(_selectedStructure->subBasinBricks.size());
    }

    /**
     * Get the number of surface component bricks in the selected structure.
     *
     * @return number of surface component bricks.
     */
    int GetSurfaceComponentBricksNb() const {
        wxASSERT(_selectedStructure);
        return static_cast<int>(_selectedStructure->surfaceComponentBricks.size());
    }

    /**
     * Get the number of processes in the selected brick.
     *
     * @return number of processes.
     */
    int GetProcessesNb() const {
        wxASSERT(_selectedBrick);
        return static_cast<int>(_selectedBrick->processes.size());
    }

    /**
     * Get the number of hydro unit splitters in the selected structure.
     *
     * @return number of hydro unit splitters.
     */
    int GetHydroUnitSplittersNb() const {
        wxASSERT(_selectedStructure);
        return static_cast<int>(_selectedStructure->hydroUnitSplitters.size());
    }

    /**
     * Get the number of sub-basin splitters in the selected structure.
     *
     * @return number of sub-basin splitters.
     */
    int GetSubBasinSplittersNb() const {
        wxASSERT(_selectedStructure);
        return static_cast<int>(_selectedStructure->subBasinSplitters.size());
    }

    /**
     * Get the solver settings.
     *
     * @return solver settings.
     */
    SolverSettings GetSolverSettings() const {
        return _solver;
    }

    /**
     * Get the timer settings.
     *
     * @return timer settings.
     */
    TimerSettings GetTimerSettings() const {
        return _timer;
    }

    /**
     * Get the hydro unit brick settings by index.
     *
     * @param index index of the hydro unit brick.
     * @return hydro unit brick settings.
     */
    BrickSettings GetHydroUnitBrickSettings(int index) const {
        wxASSERT(_selectedStructure);
        return _selectedStructure->hydroUnitBricks[index];
    }

    /**
     * Get the hydro unit brick settings by name.
     *
     * @param name name of the hydro unit brick.
     * @return hydro unit brick settings.
     */
    BrickSettings GetHydroUnitBrickSettings(const string& name) const {
        wxASSERT(_selectedStructure);

        for (auto& brick : _selectedStructure->hydroUnitBricks) {
            if (brick.name == name) {
                return brick;
            }
        }

        throw std::runtime_error("Brick not found.");
    }

    /**
     * Get the surface component brick settings by index.
     *
     * @param index index of the surface component brick.
     * @return surface component brick settings.
     */
    BrickSettings GetSurfaceComponentBrickSettings(int index) const {
        wxASSERT(_selectedStructure);
        int brickIndex = _selectedStructure->surfaceComponentBricks[index];
        return _selectedStructure->hydroUnitBricks[brickIndex];
    }

    /**
     * Get the indices of the surface component bricks.
     *
     * @return indices of the surface component bricks.
     */
    vecInt GetSurfaceComponentBricksIndices() const {
        wxASSERT(_selectedStructure);
        return _selectedStructure->surfaceComponentBricks;
    }

    /**
     * Get the indices of the land cover bricks.
     *
     * @return indices of the land cover bricks.
     */
    vecInt GetLandCoverBricksIndices() const {
        wxASSERT(_selectedStructure);
        return _selectedStructure->landCoverBricks;
    }

    /**
     * Get the names of the land cover bricks.
     *
     * @return names of the land cover bricks.
     */
    vecStr GetLandCoverBricksNames() const;

    /**
     * Get the sub basin brick settings by index.
     *
     * @param index index of the sub basin brick.
     * @return sub basin brick settings.
     */
    BrickSettings GetSubBasinBrickSettings(int index) const {
        wxASSERT(_selectedStructure);
        return _selectedStructure->subBasinBricks[index];
    }

    /**
     * Get the process settings by index.
     *
     * @param index index of the process.
     * @return process settings.
     */
    ProcessSettings GetProcessSettings(int index) const {
        wxASSERT(_selectedBrick);
        return _selectedBrick->processes[index];
    }

    /**
     * Get the splitter settings by index.
     *
     * @param index index of the splitter.
     * @return splitter settings.
     */
    SplitterSettings GetHydroUnitSplitterSettings(int index) const {
        wxASSERT(_selectedStructure);
        return _selectedStructure->hydroUnitSplitters[index];
    }

    /**
     * Get the sub basin splitter settings by index.
     *
     * @param index index of the sub basin splitter.
     * @return sub basin splitter settings.
     */
    SplitterSettings GetSubBasinSplitterSettings(int index) const {
        wxASSERT(_selectedStructure);
        return _selectedStructure->subBasinSplitters[index];
    }

    /**
     * Get the logging labels for the sub basin components
     *
     * @return logging labels for the sub basin components.
     */
    vecStr GetSubBasinLogLabels();

    /**
     * Get the generic logging labels for the sub basin components
     *
     * @return generic logging labels for the sub basin components.
     */
    vecStr GetSubBasinGenericLogLabels();

    /**
     * Get the logging labels for the hydro unit components
     *
     * @return logging labels for the hydro unit components.
     */
    vecStr GetHydroUnitLogLabels();

    /**
     * Flag to log all components.
     *
     * @param logAll true to log all components, false to log only the selected ones.
     */
    void SetLogAll(bool logAll = true) {
        if (logAll) {
            wxLogVerbose("Logging all components.");
        } else {
            wxLogVerbose("Minimal logging.");
        }
        _logAll = logAll;
    }

    /**
     * Check if all components are logged.
     *
     * @return true if all components are logged, false otherwise.
     */
    bool LogAll() {
        return _logAll;
    }

  protected:
    bool _logAll;
    vector<ModelStructure> _modelStructures;
    SolverSettings _solver;
    TimerSettings _timer;
    ModelStructure* _selectedStructure;
    BrickSettings* _selectedBrick;
    ProcessSettings* _selectedProcess;
    SplitterSettings* _selectedSplitter;

    bool LogAll(const YAML::Node& settings);
};

#endif  // HYDROBRICKS_SETTINGS_MODEL_H
