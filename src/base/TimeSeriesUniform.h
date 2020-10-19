
#ifndef FLHY_TIME_SERIES_UNIFORM_H
#define FLHY_TIME_SERIES_UNIFORM_H

#include "Includes.h"
#include "TimeSeries.h"

class TimeSeriesUniform : public TimeSeries {
  public:
    TimeSeriesUniform(VariableType type);

    ~TimeSeriesUniform() override = default;

  protected:

  private:
};

#endif  // FLHY_TIME_SERIES_UNIFORM_H
