#ifndef HYDROBRICKS_FORCING_H
#define HYDROBRICKS_FORCING_H

#include "Includes.h"
#include "TimeSeriesData.h"

class Forcing : public wxObject {
  public:
    explicit Forcing(VariableType type);

    ~Forcing() override = default;

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
     *
     * @return the value of the forcing.
     */
    double GetValue();

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
};

#endif  // HYDROBRICKS_FORCING_H
