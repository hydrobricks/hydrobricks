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
        m_manager = manager;
    }

    /**
     * Get the sporadic dates vector.
     *
     * @return the sporadic dates vector.
     */
    vecDouble GetSporadicDates() {
        return m_sporadicDates;
    }

    /**
     * Get the number of sporadic items.
     *
     * @return the number of sporadic items.
     */
    int GetSporadicItemsNb() {
        return (int)m_sporadicDates.size();
    }

    /**
     * Increment the cursor.
     */
    void IncrementCursor() {
        m_cursor++;
    }

    /**
     * Check if the action is recursive.
     *
     * @return true if the action is recursive.
     */
    bool IsRecursive() {
        return m_recursive;
    }

  protected:
    ActionsManager* m_manager;
    int m_cursor;
    vecDouble m_sporadicDates;
    bool m_recursive;
    vecInt m_recursiveMonths;
    vecInt m_recursiveDays;
};

#endif  // HYDROBRICKS_ACTION_H
