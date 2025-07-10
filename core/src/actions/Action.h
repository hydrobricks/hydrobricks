#ifndef HYDROBRICKS_ACTION_H
#define HYDROBRICKS_ACTION_H

#include "ActionsManager.h"
#include "Includes.h"

class Action : public wxObject {
  public:
    Action();

    ~Action() override = default;

    /**
     * Reset the action to its initial state.
     */
    void Reset();

    /**
     * Initialize the action.
     *
     * @return true if the initialization was successful.
     */
    bool virtual Init();

    /**
     * Apply the action for a specific date.
     *
     * @param date the date of the action.
     * @return true if the action was applied successfully.
     */
    bool virtual Apply(double date = 0);

    /**
     * Apply the recursive action, for a specific date.
     *
     * @param date the date of the action.
     * @return true if the action was applied successfully.
     */
    bool virtual ApplyIfRecursive(const Time date);

    /**
     * Get the index for insertion in the sporadic dates vector.
     *
     * @param date the date to insert.
     * @return the index for insertion.
     */
    int GetIndexForInsertion(double date);

    /**
     * Set the manager of the action.
     *
     * @param manager the manager to set.
     */
    void SetManager(ActionsManager* manager) {
        _manager = manager;
    }

    /**
     * Get the sporadic dates vector.
     *
     * @return the sporadic dates vector.
     */
    vecDouble GetSporadicDates() {
        return _sporadicDates;
    }

    /**
     * Get the number of sporadic items.
     *
     * @return the number of sporadic items.
     */
    int GetSporadicItemsNb() {
        return (int)_sporadicDates.size();
    }

    /**
     * Increment the cursor.
     */
    void IncrementCursor() {
        _cursor++;
    }

    /**
     * Check if the action is recursive.
     *
     * @return true if the action is recursive.
     */
    bool IsRecursive() {
        return _recursive;
    }

  protected:
    ActionsManager* _manager;
    int _cursor;
    vecDouble _sporadicDates;
    bool _recursive;
    vecInt _recursiveMonths;
    vecInt _recursiveDays;
};

#endif  // HYDROBRICKS_ACTION_H
