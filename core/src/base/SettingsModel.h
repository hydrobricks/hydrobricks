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
    std::vector<Parameter*> parameters;
    std::vector<VariableType> forcing;
    std::vector<OutputSettings> outputs;
};

struct SplitterSettings {
    string name;
    string type;
    vecStr logItems;
    std::vector<Parameter*> parameters;
    std::vector<VariableType> forcing;
    std::vector<OutputSettings> outputs;
};

struct BrickSettings {
    string name;
    string type;
    string parent;
    vecStr logItems;
    std::vector<Parameter*> parameters;
    std::vector<VariableType> forcing;
    std::vector<ProcessSettings> processes;
};

struct ModelStructure {
    int id;
    vecStr logItems;
    std::vector<BrickSettings> hydroUnitBricks;
    std::vector<BrickSettings> subBasinBricks;
    vecInt landCoverBricks;
    vecInt surfaceComponentBricks;
    std::vector<SplitterSettings> hydroUnitSplitters;
    std::vector<SplitterSettings> subBasinSplitters;
};

class SettingsModel : public wxObject {
  public:
    explicit SettingsModel();

    ~SettingsModel() override;

    bool GenerateStructureSocont(vecStr& landCoverTypes, vecStr& landCoverNames, int soilStorageNb = 1,
                                 const string& surfaceRunoff = "socont_runoff");

    void SetSolver(const string& solverName);

    void SetTimer(const string& start, const string& end, int timeStep, const string& timeStepUnit);

    void AddHydroUnitBrick(const string& name, const std::string& type = "storage");

    void AddSubBasinBrick(const string& name, const std::string& type = "storage");

    void AddLandCoverBrick(const string& name, const string& type);

    void AddSurfaceComponentBrick(const string& name, const string& type);

    void SetSurfaceComponentParent(const string& name);

    void AddBrickParameter(const string& name, float value, const std::string& type = "constant");

    void SetBrickParameterValue(const string& name, float value, const std::string& type = "constant");

    bool BrickHasParameter(const string& name);

    void AddBrickForcing(const string& name);

    void AddBrickProcess(const string& name, const string& type, const string& target = "", bool log = false);

    void AddProcessParameter(const string& name, float value, const std::string& type = "constant");

    void SetProcessParameterValue(const string& name, float value, const std::string& type = "constant");

    void AddProcessForcing(const string& name);

    void AddProcessOutput(const string& target);

    void SetProcessOutputsAsInstantaneous();

    void SetProcessOutputsAsStatic();

    void OutputProcessToSameBrick();

    void AddHydroUnitSplitter(const string& name, const string& type);

    void AddSubBasinSplitter(const string& name, const string& type);

    void AddSplitterParameter(const string& name, float value, const std::string& type = "constant");

    void SetSplitterParameterValue(const string& name, float value, const std::string& type = "constant");

    void AddSplitterForcing(const string& name);

    void AddSplitterOutput(const string& target, const string& fluxType = "water");

    void AddLoggingToItem(const string& itemName);

    void AddLoggingToItems(std::initializer_list<const string> items);

    void AddBrickLogging(const string& itemName);

    void AddBrickLogging(std::initializer_list<const string> items);

    void AddProcessLogging(const string& itemName);

    void AddSplitterLogging(const string& itemName);

    void GeneratePrecipitationSplitters(bool withSnow);

    void GenerateSnowpacks(const string& snowMeltProcess);

    void GenerateSnowpacksWithWaterRetention(const string& snowMeltProcess, const string& outflowProcess);

    bool SelectStructure(int id);

    void SelectHydroUnitBrick(int index);

    void SelectSubBasinBrick(int index);

    bool SelectHydroUnitBrickIfFound(const string& name);

    bool SelectSubBasinBrickIfFound(const string& name);

    void SelectHydroUnitBrick(const string& name);

    void SelectSubBasinBrick(const string& name);

    void SelectProcess(int index);

    void SelectProcess(const string& name);

    void SelectProcessWithParameter(const string& name);

    void SelectHydroUnitSplitter(int index);

    void SelectSubBasinSplitter(int index);

    bool SelectHydroUnitSplitterIfFound(const string& name);

    bool SelectSubBasinSplitterIfFound(const string& name);

    void SelectHydroUnitSplitter(const string& name);

    void SelectSubBasinSplitter(const string& name);

    bool ParseStructure(const string& path);

    bool ParseParameters(const string& path);

    bool SetParameter(const string& component, const string& name, float value);

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

    int GetSurfaceComponentBricksNb() const {
        wxASSERT(m_selectedStructure);
        return int(m_selectedStructure->surfaceComponentBricks.size());
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

    BrickSettings GetSurfaceComponentBrickSettings(int index) const {
        wxASSERT(m_selectedStructure);
        int brickIndex = m_selectedStructure->surfaceComponentBricks[index];
        return m_selectedStructure->hydroUnitBricks[brickIndex];
    }

    vecInt GetSurfaceComponentBricksIndices() const {
        wxASSERT(m_selectedStructure);
        return m_selectedStructure->surfaceComponentBricks;
    }

    vecInt GetLandCoverBricksIndices() const {
        wxASSERT(m_selectedStructure);
        return m_selectedStructure->landCoverBricks;
    }

    vecStr GetLandCoverBricksNames() const;

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

    void SetLogAll(bool logAll = true) {
        if (logAll) {
            wxLogVerbose("Logging all components.");
        } else {
            wxLogVerbose("Minimal logging.");
        }
        m_logAll = logAll;
    }

    bool LogAll() {
        return m_logAll;
    }

  protected:
    bool m_logAll;
    std::vector<ModelStructure> m_modelStructures;
    SolverSettings m_solver;
    TimerSettings m_timer;
    ModelStructure* m_selectedStructure;
    BrickSettings* m_selectedBrick;
    ProcessSettings* m_selectedProcess;
    SplitterSettings* m_selectedSplitter;

    vecStr ParseLandCoverNames(const YAML::Node& settings);

    vecStr ParseLandCoverTypes(const YAML::Node& settings);

    string ParseSolver(const YAML::Node& settings);

    bool LogAll(const YAML::Node& settings);
};

#endif  // HYDROBRICKS_SETTINGS_MODEL_H
