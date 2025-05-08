#ifndef HYDROBRICKS_ACTION_UPDATER_H
#define HYDROBRICKS_ACTION_UPDATER_H

#include "HydroUnit.h"
#include "Includes.h"

class ModelHydro;
class Action;

class ActionsManager : public wxObject {
  public:
    ActionsManager();

    ~ActionsManager() override = default;

    void Reset();

    ModelHydro* GetModel() {
        return m_model;
    }

    void SetModel(ModelHydro* model);

    bool AddAction(Action* action);

    int GetActionsNb();

    int GetActionItemsNb();

    void DateUpdate(double date);

    HydroUnit* GetHydroUnitById(int id);

    vecDouble GetDates() {
        return m_dates;
    }

  protected:
    bool m_active;
    ModelHydro* m_model;
    int m_cursorManager;
    vector<Action*> m_actions;
    vecDouble m_dates;
    vecInt m_actionIndices;

  private:
};

#endif  // HYDROBRICKS_ACTION_UPDATER_H
