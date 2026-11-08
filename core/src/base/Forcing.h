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
     * Check whether time series data was attached to the forcing.
     *
     * @return true if data is attached.
     */
    [[nodiscard]] bool HasData() const {
        return _timeSeriesData != nullptr;
    }

    /**
     * Check whether the forcing accumulates over the time step (a depth of water, given
     * per step: precipitation, PET) rather than being an instantaneous or averaged value
     * that does not depend on the step length (temperature, radiation).
     *
     * Only a cumulative forcing may be divided by the step length to give a rate. The
     * custom variables are treated as non-cumulative: their meaning is the user's, and
     * scaling one that was not meant to be scaled would silently corrupt it.
     *
     * @return true if the value is an amount accumulated over the time step.
     */
    [[nodiscard]] bool IsCumulative() const {
        return _type == VariableType::Precipitation || _type == VariableType::PET;
    }

    /**
     * Get the value of the forcing at the current time in the simulation.
     * Returns the dynamic override if one has been set this timestep, otherwise the time-series value.
     *
     * @return the value of the forcing.
     */
    virtual double GetValue() const;

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
    [[nodiscard]] virtual bool IsValid() const;

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

/**
 * A forcing read by the subbasin-level components: the area-weighted mean of the same
 * forcing in the subbasin's hydro units (e.g. a catchment-mean PET for a production store
 * lumped over the subbasin while the snow runs per elevation band). A process may still
 * override the value for the current time step (UpdateValue), as on a hydro unit.
 */
class AggregatedForcing : public Forcing {
  public:
    explicit AggregatedForcing(VariableType type);

    /**
     * Add a hydro unit forcing to the mean.
     *
     * @param source the forcing of the same type in a hydro unit (non-owning).
     * @param weight its weight (the area share of the unit in the subbasin).
     */
    void AddSource(Forcing* source, double weight);

    /**
     * Get the area-weighted mean of the hydro unit values, or the override set this time step.
     *
     * @return the value of the forcing.
     */
    double GetValue() const override;

    /**
     * Check that the forcing has sources, all of them valid.
     *
     * @return true if the forcing is valid, false otherwise.
     */
    [[nodiscard]] bool IsValid() const override;

  private:
    std::vector<std::pair<Forcing*, double>> _sources;  // non-owning: the hydro units own them
};

#endif  // HYDROBRICKS_FORCING_H
