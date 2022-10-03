#ifndef HYDROBRICKS_SETTINGS_MODEL_H
#define HYDROBRICKS_SETTINGS_MODEL_H

#include <yaml-cpp/yaml.h>

#include "Includes.h"
#include "Parameter.h"

struct SolverSettings {
    std::string name;
};

struct TimerSettings {
    std::string start;
    std::string end;
    int timeStep = 1;
    std::string timeStepUnit;
};

struct OutputSettings {
    std::string target;
    std::string fluxType = "water";
    bool instantaneous = false;
};

struct ProcessSettings {
    std::string name;
    std::string type;
    vecStr logItems;
    std::vector<Parameter*> parameters;
    std::vector<VariableType> forcing;
    std::vector<OutputSettings> outputs;
};

struct SplitterSettings {
    std::string name;
    std::string type;
    vecStr logItems;
    std::vector<Parameter*> parameters;
    std::vector<VariableType> forcing;
    std::vector<OutputSettings> outputs;
};

struct BrickSettings {
    std::string name;
    std::string type;
    vecStr logItems;
    vecStr relatedSurfaceBricks;
    std::vector<Parameter*> parameters;
    std::vector<VariableType> forcing;
    std::vector<ProcessSettings> processes;
};

struct ModelStructure {
    int id;
    vecStr logItems;
    std::vector<BrickSettings> surfaceBricks;
    std::vector<BrickSettings> hydroUnitBricks;
    std::vector<BrickSettings> subBasinBricks;
    std::vector<SplitterSettings> hydroUnitSplitters;
    std::vector<SplitterSettings> subBasinSplitters;
};

class SettingsModel : public wxObject {
  public:
    explicit SettingsModel();

    ~SettingsModel() override;

    void SetSolver(const std::string& solverName);

    void SetTimer(const std::string& start, const std::string& end, int timeStep, const std::string& timeStepUnit);

    void AddHydroUnitBrick(const std::string& name, const std::string& type);

    void AddSubBasinBrick(const std::string& name, const std::string& type);

    void AddSurfaceBrick(const std::string& name, const std::string& type);

    void AddToRelatedSurfaceBrick(const std::string& name);

    void AddBrickParameter(const std::string& name, float value, const std::string& type = "Constant");

    void SetBrickParameterValue(const std::string& name, float value, const std::string& type = "Constant");

    bool BrickHasParameter(const std::string& name);

    void AddBrickForcing(const std::string& name);

    void AddBrickProcess(const std::string& name, const std::string& type);

    void AddProcessParameter(const std::string& name, float value, const std::string& type = "Constant");

    void SetProcessParameterValue(const std::string& name, float value, const std::string& type = "Constant");

    void AddProcessForcing(const std::string& name);

    void AddProcessOutput(const std::string& target);

    void SetProcessOutputsAsInstantaneous();

    void OutputProcessToSameBrick();

    void AddHydroUnitSplitter(const std::string& name, const std::string& type);

    void AddSubBasinSplitter(const std::string& name, const std::string& type);

    void AddSplitterParameter(const std::string& name, float value, const std::string& type = "Constant");

    void SetSplitterParameterValue(const std::string& name, float value, const std::string& type = "Constant");

    void AddSplitterForcing(const std::string& name);

    void AddSplitterOutput(const std::string& target, const std::string& fluxType = "water");

    void AddLoggingToItem(const std::string& itemName);

    void AddBrickLogging(const std::string& itemName);

    void AddProcessLogging(const std::string& itemName);

    void AddSplitterLogging(const std::string& itemName);

    void GeneratePrecipitationSplitters(bool withSnow);

    void GenerateSnowpacks(const std::string& snowMeltProcess);

    void GenerateSnowpacksWithWaterRetention(const std::string& snowMeltProcess, const std::string& outflowProcess);

    void GenerateSurfaceComponentBricks(bool withSnow);

    void GenerateSurfaceBricks();

    bool SelectStructure(int id);

    void SelectHydroUnitBrick(int index);

    void SelectSubBasinBrick(int index);

    bool SelectHydroUnitBrickIfFound(const std::string& name);

    bool SelectSubBasinBrickIfFound(const std::string& name);

    void SelectHydroUnitBrick(const std::string& name);

    void SelectSubBasinBrick(const std::string& name);

    void SelectProcess(int index);

    void SelectProcess(const std::string& name);

    void SelectProcessWithParameter(const std::string& name);

    void SelectHydroUnitSplitter(int index);

    void SelectSubBasinSplitter(int index);

    bool SelectHydroUnitSplitterIfFound(const std::string& name);

    bool SelectSubBasinSplitterIfFound(const std::string& name);

    void SelectHydroUnitSplitter(const std::string& name);

    void SelectSubBasinSplitter(const std::string& name);

    bool ParseStructure(const std::string& path);

    bool ParseParameters(const std::string& path);

    int GetStructuresNb() const {
        return int(m_modelStructures.size());
    }

    int GetHydroUnitBricksNb() const {
        wxASSERT(m_selectedStructure);
        return int(m_selectedStructure->hydroUnitBricks.size());
    }

    int GetSubBasinBricksNb() const {
        wxASSERT(m_selectedStructure);
        return int(m_selectedStructure->subBasinBricks.size());
    }

    int GetProcessesNb() const {
        wxASSERT(m_selectedBrick);
        return int(m_selectedBrick->processes.size());
    }

    int GetHydroUnitSplittersNb() const {
        wxASSERT(m_selectedStructure);
        return int(m_selectedStructure->hydroUnitSplitters.size());
    }

    int GetSubBasinSplittersNb() const {
        wxASSERT(m_selectedStructure);
        return int(m_selectedStructure->subBasinSplitters.size());
    }

    SolverSettings GetSolverSettings() const {
        return m_solver;
    }

    TimerSettings GetTimerSettings() const {
        return m_timer;
    }

    BrickSettings GetHydroUnitBrickSettings(int index) const {
        wxASSERT(m_selectedStructure);
        return m_selectedStructure->hydroUnitBricks[index];
    }

    BrickSettings GetSubBasinBrickSettings(int index) const {
        wxASSERT(m_selectedStructure);
        return m_selectedStructure->subBasinBricks[index];
    }

    ProcessSettings GetProcessSettings(int index) const {
        wxASSERT(m_selectedBrick);
        return m_selectedBrick->processes[index];
    }

    SplitterSettings GetHydroUnitSplitterSettings(int index) const {
        wxASSERT(m_selectedStructure);
        return m_selectedStructure->hydroUnitSplitters[index];
    }

    SplitterSettings GetSubBasinSplitterSettings(int index) const {
        wxASSERT(m_selectedStructure);
        return m_selectedStructure->subBasinSplitters[index];
    }

    vecStr GetSubBasinLogLabels();

    vecStr GetSubBasinGenericLogLabels();

    vecStr GetHydroUnitLogLabels();

  protected:
    std::vector<ModelStructure> m_modelStructures;
    SolverSettings m_solver;
    TimerSettings m_timer;
    ModelStructure* m_selectedStructure;
    BrickSettings* m_selectedBrick;
    ProcessSettings* m_selectedProcess;
    SplitterSettings* m_selectedSplitter;

    vecStr ParseSurfaceNames(const YAML::Node& settings);

    vecStr ParseSurfaceTypes(const YAML::Node& settings);

    std::string ParseSolver(const YAML::Node& settings);

    bool LogAll(const YAML::Node& settings);

    bool GenerateStructureSocont(const YAML::Node& settings);
};

#endif  // HYDROBRICKS_SETTINGS_MODEL_H
