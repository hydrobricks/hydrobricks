#ifndef HYDROBRICKS_SETTINGS_MODEL_H
#define HYDROBRICKS_SETTINGS_MODEL_H

#include "yaml-cpp/yaml.h"

#include "Includes.h"
#include "Parameter.h"

struct SolverSettings {
    wxString name;
};

struct TimerSettings {
    wxString start;
    wxString end;
    int timeStep = 1;
    wxString timeStepUnit;
};

struct OutputSettings {
    wxString target;
    wxString fluxType = "water";
    bool withWeighting = false;
    bool instantaneous = false;
};

struct ProcessSettings {
    wxString name;
    wxString type;
    vecStr logItems;
    std::vector<Parameter*> parameters;
    std::vector<VariableType> forcing;
    std::vector<OutputSettings> outputs;
};

struct SplitterSettings {
    wxString name;
    wxString type;
    vecStr logItems;
    std::vector<Parameter*> parameters;
    std::vector<VariableType> forcing;
    std::vector<OutputSettings> outputs;
};

struct BrickSettings {
    wxString name;
    wxString type;
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

    void SetSolver(const wxString &solverName);

    void SetTimer(const wxString &start, const wxString &end, int timeStep, const wxString &timeStepUnit);

    void AddHydroUnitBrick(const wxString &name, const wxString &type);

    void AddSubBasinBrick(const wxString &name, const wxString &type);

    void AddSurfaceBrick(const wxString &name, const wxString &type);

    void AddToRelatedSurfaceBrick(const wxString &name);

    void AddBrickParameter(const wxString &name, float value, const wxString &type = "Constant");

    void AddBrickForcing(const wxString &name);

    void AddBrickProcess(const wxString &name, const wxString &type);

    void AddProcessParameter(const wxString &name, float value, const wxString &type = "Constant");

    void AddProcessForcing(const wxString &name);

    void AddProcessOutput(const wxString &target, bool withWeighting = false);

    void OutputProcessToSameBrick();

    void AddHydroUnitSplitter(const wxString &name, const wxString &type);

    void AddSubBasinSplitter(const wxString &name, const wxString &type);

    void AddSplitterParameter(const wxString &name, float value, const wxString &type = "Constant");

    void AddSplitterForcing(const wxString &name);

    void AddSplitterOutput(const wxString &target, const wxString &fluxType = "water");

    void AddLoggingToItem(const wxString &itemName);

    void AddBrickLogging(const wxString &itemName);

    void AddProcessLogging(const wxString &itemName);

    void AddSplitterLogging(const wxString &itemName);

    void GeneratePrecipitationSplitters(bool withSnow);

    void GenerateSnowpacks(const wxString& snowMeltProcess);

    void GenerateSnowpacksWithWaterRetention(const wxString& snowMeltProcess, const wxString& outflowProcess);

    void GenerateSurfaceComponentBricks(bool withSnow);

    void GenerateSurfaceBricks();

    bool SelectStructure(int id);

    void SelectHydroUnitBrick(int index);

    void SelectSubBasinBrick(int index);

    void SelectHydroUnitBrick(const wxString &name);

    void SelectSubBasinBrick(const wxString &name);

    void SelectProcess(int index);

    void SelectProcess(const wxString &name);

    void SelectHydroUnitSplitter(int index);

    void SelectSubBasinSplitter(int index);

    void SelectHydroUnitSplitter(const wxString &name);

    void SelectSubBasinSplitter(const wxString &name);

    bool Parse(const wxString &path);

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

    vecStr GetHydroUnitLogLabels();

  protected:
    std::vector<ModelStructure> m_modelStructures;
    SolverSettings m_solver;
    TimerSettings m_timer;
    ModelStructure* m_selectedStructure;
    BrickSettings* m_selectedBrick;
    ProcessSettings* m_selectedProcess;
    SplitterSettings* m_selectedSplitter;

    vecStr ParseSurfaceNames(const YAML::Node &settings);

    vecStr ParseSurfaceTypes(const YAML::Node &settings);

    wxString ParseSolver(const YAML::Node &settings);

    bool LogAll(const YAML::Node &settings);

    bool GenerateStructureSocont(const YAML::Node &settings);
};

#endif  // HYDROBRICKS_SETTINGS_MODEL_H
