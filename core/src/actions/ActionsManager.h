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

    /**
     * Reset the actions manager.
     */
    void Reset();

    /**
     * Get the associated model.
     *
     * @return pointer to the model.
     */
    ModelHydro* GetModel() {
        return m_model;
    }

    /**
     * Set the associated model.
     *
     * @param model pointer to the model.
     */
    void SetModel(ModelHydro* model);

    /**
     * Add an action to the manager.
     *
     * @param action pointer to the action.
     * @return true if the action was added successfully.
     */
    bool AddAction(Action* action);

    /**
     * Get the number of actions.
     *
     * @return number of actions.
     */
    int GetActionsNb();

    /**
     * Get the number of sporadic action items (i.e., actions that are not recursive).
     *
     * @return number of sporadic action items.
     */
    int GetSporadicActionItemsNb();

    /**
     * Update the date during the simulation. Triggers the actions that are scheduled for the current date.
     *
     * @param date corresponding date of the simulation.
     */
    void DateUpdate(double date);

    /**
     * Get hydro unit by ID.
     *
     * @param id ID of the hydro unit.
     * @return pointer to the hydro unit.
     */
    HydroUnit* GetHydroUnitById(int id);

    /**
     * Get the list of sporadic action dates (i.e., actions that are not recursive).
     *
     * @return vector of sporadic action dates.
     */
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
