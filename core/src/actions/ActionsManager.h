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
    ModelHydro* GetModel() const {
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
    int GetActionCount() const;

    /**
     * Get the number of sporadic action items (i.e., actions that are not recursive).
     *
     * @return number of sporadic action items.
     */
    int GetSporadicActionItemCount() const;

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
    const vecDouble& GetSporadicActionDates() const {
        return _sporadicActionDates;
    }

    /**
     * Check if the actions manager is valid.
     * Verifies that the manager is properly configured with a model.
     *
     * @return true if the actions manager is valid, false otherwise.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * Validate the actions manager.
     * Throws an exception if the actions manager is invalid.
     *
     * @throws ModelConfigError if validation fails.
     */
    void Validate() const;

  protected:
    ModelHydro* _model;  // non-owning reference
    int _cursorManager;
    vector<Action*> _actions;
    vecDouble _sporadicActionDates;
    vecInt _sporadicActionIndices;
    vecInt _recursiveActionIndices;

  private:
};

#endif  // HYDROBRICKS_ACTION_UPDATER_H
