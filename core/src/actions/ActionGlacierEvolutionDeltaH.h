#ifndef HYDROBRICKS_ACTION_GLACIER_EVOLUTION_DELTA_H_H
#define HYDROBRICKS_ACTION_GLACIER_EVOLUTION_DELTA_H_H

#include "Action.h"
#include "Includes.h"

class ActionGlacierEvolutionDeltaH : public Action {
  public:
    ActionGlacierEvolutionDeltaH();

    ~ActionGlacierEvolutionDeltaH() override = default;

    /**
     * Add lookup tables for glacier evolution.
     *
     * @param month month of the year (1-12).
     * @param landCoverName name of the land cover associated with the action. It is used to identify the specific
     * glacier 'name' as we can have multiple glacier land covers in the model.
     * @param hydroUnitIds vector of hydro unit IDs.
     * @param areas matrix of changes in area (rows: iterations, columns: hydro units).
     * @param volumes matrix of changes in volume (rows: iterations, columns: hydro units).
     */
    void AddLookupTables(int month, const string& landCoverName, const axi& hydroUnitIds, const axxd& areas,
                         const axxd& volumes);

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
    axi GetHydroUnitIds() {
        return _hydroUnitIds;
    }

    /**
     * Get the lookup table for the glacier area.
     *
     * @return The lookup table for the glacier area.
     */
    axxd GetLookupTableArea() {
        return _tableArea;
    }

    /**
     * Get the lookup table for the glacier volume.
     *
     * @return The lookup table for the glacier volume.
     */
    axxd GetLookupTableVolume() {
        return _tableVolume;
    }

  protected:
    int _lastRow{0};
    string _landCoverName;
    axi _hydroUnitIds;
    axxd _tableArea;
    axxd _tableVolume;
    double _initialGlacierWE{0.0};
};

#endif  // HYDROBRICKS_ACTION_GLACIER_EVOLUTION_DELTA_H_H
