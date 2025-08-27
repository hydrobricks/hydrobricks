#ifndef HYDROBRICKS_ACTION_UPDATER_H
#define HYDROBRICKS_ACTION_UPDATER_H

#include "HydroUnit.h"
#include "Includes.h"

class SubBasin;
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
        return _model;
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
     * Get the sub basin associated with the model.
     *
     * @return pointer to the sub basin.
     */
    SubBasin* GetSubBasin() const;

    /**
     * Get hydro unit by ID.
     *
     * @param id ID of the hydro unit.
     * @return pointer to the hydro unit.
     */
    HydroUnit* GetHydroUnitById(int id) const;

    /**
     * Get the list of sporadic action dates (i.e., actions that are not recursive).
     *
     * @return vector of sporadic action dates.
     */
    vecDouble GetSporadicActionDates() {
        return _sporadicActionDates;
    }

  protected:
    ModelHydro* _model;
    int _cursorManager;
    vector<Action*> _actions;
    vecDouble _sporadicActionDates;
    vecInt _sporadicActionIndices;
    vecInt _recursiveActionIndices;

  private:
};

#endif  // HYDROBRICKS_ACTION_UPDATER_H
