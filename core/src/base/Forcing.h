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
    VariableType GetType() {
        return m_type;
    }

    /**
     * Get the value of the forcing at the current time in the simulation.
     *
     * @return the value of the forcing.
     */
    double GetValue();

  protected:
    VariableType m_type;
    TimeSeriesData* m_timeSeriesData;
};

#endif  // HYDROBRICKS_FORCING_H
