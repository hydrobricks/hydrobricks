#ifndef HYDROBRICKS_ACTION_LAND_COVER_CHANGE_H
#define HYDROBRICKS_ACTION_LAND_COVER_CHANGE_H

#include "Action.h"
#include "Includes.h"

class ActionLandCoverChange : public Action {
  public:
    ActionLandCoverChange();

    ~ActionLandCoverChange() override = default;

    /**
     * Add a land cover change to the action.
     *
     * @param date date of the change.
     * @param hydroUnitId ID of the hydro unit.
     * @param landCoverName name of the land cover.
     * @param area area of the land cover.
     */
    void AddChange(double date, int hydroUnitId, const string& landCoverName, double area);

    /**
     * Reset the action to its initial state.
     */
    void Reset() override;

    /**
     * Apply the action to the model.
     *
     * @param date date of the action.
     * @return true if the action was applied successfully.
     */
    bool Apply(double date) override;

    /**
     * Get the number of changes in the action.
     *
     * @return the number of changes in the action.
     */
    int GetChangesNb() const {
        return static_cast<int>(_sporadicDates.size());
    }

    /**
     * Get the number of land covers in the action.
     *
     * @return the number of land covers in the action.
     */
    int GetLandCoversNb() const {
        return static_cast<int>(_landCoverNames.size());
    }

  protected:
    vecInt _hydroUnitIds;
    vecInt _landCoverIds;
    vecStr _landCoverNames;
    vecDouble _areas;

  private:
    int GetLandCoverId(const string& landCoverName);
};

#endif  // HYDROBRICKS_ACTION_LAND_COVER_CHANGE_H
