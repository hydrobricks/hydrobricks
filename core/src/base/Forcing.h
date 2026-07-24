#ifndef HYDROBRICKS_FORCING_H
#define HYDROBRICKS_FORCING_H

#include "Includes.h"
#include "TimeSeriesData.h"

class Forcing {
  public:
    explicit Forcing(VariableType type);

    virtual ~Forcing() = default;

    /**
     * Attach time series data to the forcing.
     *
     * @param timeSeriesData pointer to the time series data.
     */
    void AttachTimeSeriesData(TimeSeriesData* timeSeriesData);

    /**
     * Get the type of the forcing.
     *
     * @return the type of the forcing.
     */
    VariableType GetType() const {
        return _type;
    }

    /**
     * Get the value of the forcing at the current time in the simulation.
     * Returns the dynamic override if one has been set this timestep, otherwise the time-series value.
     *
     * @return the value of the forcing.
     */
    double GetValue() const;

    /**
     * Override the forcing value for the current timestep.
     * Called by a process (e.g. interception) to publish a derived quantity (e.g. En = E − P)
     * so that downstream processes on the same HydroUnit see the updated value.
     *
     * @param value the override value.
     */
    void UpdateValue(double value);

    /**
     * Clear the dynamic override set by UpdateValue().
     * Called by ModelHydro at the end of each timestep via HydroUnit::ResetForcingUpdates().
     */
    void ResetUpdate();

    /**
     * Check if the forcing is valid.
     * Verifies that time series data is attached.
     *
     * @return true if the forcing is valid, false otherwise.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * Validate the forcing.
     * Throws an exception if the forcing is invalid.
     *
     * @throws ModelConfigError if validation fails.
     */
    void Validate() const;

  protected:
    VariableType _type;
    TimeSeriesData* _timeSeriesData;  // non-owning reference
    double _updatedValue{0.0};
    bool _hasUpdatedValue{false};
};

#endif  // HYDROBRICKS_FORCING_H
