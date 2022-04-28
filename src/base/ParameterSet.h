#ifndef HYDROBRICKS_PARAMETER_SET_H
#define HYDROBRICKS_PARAMETER_SET_H

#include "Includes.h"
#include "Parameter.h"

struct SolverSettings {
    wxString name;
};

struct TimerSettings {
    wxString start;
    wxString end;
    int timeStep;
    wxString timeStepUnit;
};

struct BrickOutputSettings {
    wxString target;
    wxString type;
};

struct BrickSettings {
    wxString name;
    wxString type;
    std::vector<Parameter*> parameters;
    std::vector<VariableType> forcing;
    std::vector<BrickOutputSettings> outputs;
};

struct ModelStructure {
    int id;
    std::vector<BrickSettings> bricks;
};

class ParameterSet : public wxObject {
  public:
    explicit ParameterSet();

    ~ParameterSet() override;

    void SetSolver(const wxString &solverName);

    void SetTimer(const wxString &start, const wxString &end, int timeStep, const wxString &timeStepUnit);

    void AddBrick(const wxString &name, const wxString &type);

    void AddParameterToCurrentBrick(const wxString &name, float value, const wxString &type = "Constant");

    void AddForcingToCurrentBrick(const wxString &name);

    void AddOutputToCurrentBrick(const wxString &target, const wxString &type = "Direct");

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

  protected:
    std::vector<ModelStructure> m_modelStructures;
    SolverSettings m_solver;
    TimerSettings m_timer;
    ModelStructure* m_selectedStructure;
    BrickSettings* m_selectedBrick;

  private:
};

#endif  // HYDROBRICKS_PARAMETER_SET_H
