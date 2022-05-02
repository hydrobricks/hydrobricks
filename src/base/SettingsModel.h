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

struct BrickOutputSettings {
    wxString target;
    wxString type;
};

struct BrickSettings {
    wxString name;
    wxString type;
    vecStr logItems;
    std::vector<Parameter*> parameters;
    std::vector<VariableType> forcing;
    std::vector<BrickOutputSettings> outputs;
};

struct ModelStructure {
    int id;
    vecStr logItems;
    std::vector<BrickSettings> bricks;
};

class SettingsModel : public wxObject {
  public:
    explicit SettingsModel();

    ~SettingsModel() override;

    void SetSolver(const wxString &solverName);

    void SetTimer(const wxString &start, const wxString &end, int timeStep, const wxString &timeStepUnit);

    void AddBrick(const wxString &name, const wxString &type);

    void AddParameterToCurrentBrick(const wxString &name, float value, const wxString &type = "Constant");

    void AddForcingToCurrentBrick(const wxString &name);

    void AddOutputToCurrentBrick(const wxString &target, const wxString &type = "Direct");

    void AddLoggingToCurrentBrick(const wxString& itemName);

    void AddLoggingToItem(const wxString& itemName);

    bool SelectStructure(int id);

    int GetStructuresNb() const {
        return int(m_modelStructures.size());
    }

    int GetBricksNb() const {
        wxASSERT(m_selectedStructure);
        return int(m_selectedStructure->bricks.size());
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

    vecStr GetAggregatedLogLabels();

    vecStr GetHydroUnitLogLabels();

  protected:
    std::vector<ModelStructure> m_modelStructures;
    SolverSettings m_solver;
    TimerSettings m_timer;
    ModelStructure* m_selectedStructure;
    BrickSettings* m_selectedBrick;
};

#endif  // HYDROBRICKS_SETTINGS_MODEL_H
