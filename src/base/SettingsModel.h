#ifndef HYDROBRICKS_SETTINGS_MODEL_H
#define HYDROBRICKS_SETTINGS_MODEL_H

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
    bool withWeighting = false;
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
    bool withSnow = false;
    wxString snowMeltProcess;
    vecStr logItems;
    std::vector<BrickSettings> surfaceBricks;
    std::vector<BrickSettings> bricks;
    std::vector<SplitterSettings> splitters;
};

class SettingsModel : public wxObject {
  public:
    explicit SettingsModel();

    ~SettingsModel() override;

    void SetSolver(const wxString &solverName);

    void SetTimer(const wxString &start, const wxString &end, int timeStep, const wxString &timeStepUnit);

    void AddBrick(const wxString &name, const wxString &type);

    void AddSurfaceBrick(const wxString &name, const wxString &type);

    void AddToRelatedSurfaceBrick(const wxString &name);

    void AddParameterToCurrentBrick(const wxString &name, float value, const wxString &type = "Constant");

    void AddForcingToCurrentBrick(const wxString &name);

    void AddProcessToCurrentBrick(const wxString &name, const wxString &type);

    void AddParameterToCurrentProcess(const wxString &name, float value, const wxString &type = "Constant");

    void AddForcingToCurrentProcess(const wxString &name);

    void AddOutputToCurrentProcess(const wxString &target, bool withWeighting = false);

    void AddSplitter(const wxString &name, const wxString &type);

    void AddParameterToCurrentSplitter(const wxString &name, float value, const wxString &type = "Constant");

    void AddForcingToCurrentSplitter(const wxString &name);

    void AddOutputToCurrentSplitter(const wxString &target);

    void AddLoggingToItem(const wxString &itemName);

    void AddLoggingToCurrentBrick(const wxString &itemName);

    void AddLoggingToCurrentProcess(const wxString &itemName);

    void AddLoggingToCurrentSplitter(const wxString &itemName);

    void EnableSnow(const wxString &meltProcess);

    void GenerateSurfaceComponents();

    bool SelectStructure(int id);

    void SelectBrick(int index);

    void SelectBrick(const wxString &name);

    void SelectProcess(int index);

    void SelectProcess(const wxString &name);

    void SelectSplitter(int index);

    void SelectSplitter(const wxString &name);

    bool Parse(const wxString &path);

    int GetStructuresNb() const {
        return int(m_modelStructures.size());
    }

    int GetBricksNb() const {
        wxASSERT(m_selectedStructure);
        return int(m_selectedStructure->bricks.size());
    }

    int GetProcessesNb() const {
        wxASSERT(m_selectedBrick);
        return int(m_selectedBrick->processes.size());
    }

    int GetSplittersNb() const {
        wxASSERT(m_selectedStructure);
        return int(m_selectedStructure->splitters.size());
    }

    SolverSettings GetSolverSettings() const {
        return m_solver;
    }

    TimerSettings GetTimerSettings() const {
        return m_timer;
    }

    BrickSettings GetBrickSettings(int index) const {
        wxASSERT(m_selectedStructure);
        return m_selectedStructure->bricks[index];
    }

    ProcessSettings GetProcessSettings(int index) const {
        wxASSERT(m_selectedBrick);
        return m_selectedBrick->processes[index];
    }

    SplitterSettings GetSplitterSettings(int index) const {
        wxASSERT(m_selectedStructure);
        return m_selectedStructure->splitters[index];
    }

    vecStr GetAggregatedLogLabels();

    vecStr GetHydroUnitLogLabels();

  protected:
    std::vector<ModelStructure> m_modelStructures;
    SolverSettings m_solver;
    TimerSettings m_timer;
    ModelStructure* m_selectedStructure;
    BrickSettings* m_selectedBrick;
    ProcessSettings* m_selectedProcess;
    SplitterSettings* m_selectedSplitter;
};

#endif  // HYDROBRICKS_SETTINGS_MODEL_H
