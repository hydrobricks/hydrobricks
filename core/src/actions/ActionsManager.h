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

    int GetSporadicActionItemsNb();

    void DateUpdate(double date);

    HydroUnit* GetHydroUnitById(int id);

    vecDouble GetSporadicActionDates() {
        return m_sporadicActionDates;
    }

  protected:
    ModelHydro* m_model;
    int m_cursorManager;
    vector<Action*> m_actions;
    vecDouble m_sporadicActionDates;
    vecInt m_sporadicActionIndices;
    vecInt m_recursiveActionIndices;

  private:
};

#endif  // HYDROBRICKS_ACTION_UPDATER_H
