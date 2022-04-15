
#ifndef HYDROBRICKS_TIME_SERIES_UNIFORM_H
#define HYDROBRICKS_TIME_SERIES_UNIFORM_H

#include "Includes.h"
#include "TimeSeries.h"

class TimeSeriesUniform : public TimeSeries {
  public:
    TimeSeriesUniform(VariableType type);

    ~TimeSeriesUniform() override = default;

  protected:
    TimeSeriesData* m_data;

  private:
};

#endif  // HYDROBRICKS_TIME_SERIES_UNIFORM_H
