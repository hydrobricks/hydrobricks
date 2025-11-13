#ifndef HYDROBRICKS_ACTION_GLACIER_SNOW_TO_ICE_TRANSFORMATION_H
#define HYDROBRICKS_ACTION_GLACIER_SNOW_TO_ICE_TRANSFORMATION_H

#include "Action.h"
#include "Includes.h"

class ActionGlacierSnowToIceTransformation : public Action {
  public:
    /**
     * Constructor for the ActionGlacierSnowToIceTransformation class.
     *
     * @param month month of the year (1-12).
     * @param day day of the month (1-31).
     * @param landCoverName name of the land cover associated with the action. It is used to identify the specific
     * glacier 'name' as we can have multiple glacier land covers in the model.
     */
    ActionGlacierSnowToIceTransformation(int month, int day, const string& landCoverName);

    ~ActionGlacierSnowToIceTransformation() override = default;

    /**
     * Initialize the action.
     */
    [[nodiscard]] bool Init() override;

    /**
     * Reset the action to its initial state.
     */
    void Reset() override;

    /**
     * Apply the action.
     *
     * @param date date of the action.
     * @return true if the action was applied successfully.
     */
    [[nodiscard]] bool Apply(double date) override;

    /**
     * Get the land cover name (glacier name) associated with the action.
     *
     * @return land cover name (glacier name).
     */
    string GetLandCoverName() {
        return _landCoverName;
    }

    /**
     * Get the hydro unit IDs associated with the action.
     *
     * @return vector of hydro unit IDs.
     */
    vecInt GetHydroUnitIds() {
        return _hydroUnitIds;
    }

  protected:
    string _landCoverName;
    vecInt _hydroUnitIds;
};

#endif  // HYDROBRICKS_ACTION_GLACIER_SNOW_TO_ICE_TRANSFORMATION_H
