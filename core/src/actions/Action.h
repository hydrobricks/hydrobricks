#ifndef HYDROBRICKS_ACTION_H
#define HYDROBRICKS_ACTION_H

#include "ActionsManager.h"
#include "Includes.h"

class Action : public wxObject {
  public:
    Action();

    ~Action() override = default;

    /**
     * Initialize the action.
     *
     * @return true if the initialization was successful.
     */
    virtual bool Init();

    /**
     * Reset the action to its initial state.
     */
    virtual void Reset();

    /**
     * Reset the action cursor.
     */
    void ResetCursor();

    /**
     * Add a recursive date for the action.
     *
     * @param month the month of the year (1-12).
     * @param day the day of the month (1-31).
     */
    void AddRecursiveDate(int month, int day);

    /**
     * Apply the action for a specific date.
     *
     * @param date the date of the action.
     * @return true if the action was applied successfully.
     */
    virtual bool Apply(double date = 0);

    /**
     * Apply the recursive action, for a specific date.
     *
     * @param date the date of the action.
     * @return true if the action was applied successfully.
     */
    virtual bool ApplyIfRecursive(const Time date);

    /**
     * Get the index for insertion in the sporadic dates vector.
     *
     * @param date the date to insert.
     * @return the index for insertion.
     */
    int GetIndexForInsertion(double date);

    /**
     * Check if the land cover area fraction is valid.
     *
     * @param name the name of the land cover.
     * @param id the ID of the hydro unit.
     * @param fraction the area fraction to check.
     * @param unitArea the area of the hydro unit.
     * @param lcArea the area of the land cover.
     * @return the corrected area fraction if it was invalid, or the original fraction if it was valid.
     */
    double CheckLandCoverAreaFraction(const string& name, int id, double fraction, double unitArea, double lcArea);

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
    int GetSporadicItemCount() {
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
    [[nodiscard]] bool IsRecursive() const {
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
