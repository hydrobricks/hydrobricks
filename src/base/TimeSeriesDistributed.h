
#ifndef FLHY_TIME_SERIES_DISTRIBUTED_H
#define FLHY_TIME_SERIES_DISTRIBUTED_H

#include "Includes.h"
#include "TimeSeries.h"

class TimeSeriesDistributed : public TimeSeries {
  public:
    TimeSeriesDistributed(VariableType type);

    ~TimeSeriesDistributed() override = default;

  protected:

  private:
};

#endif  // FLHY_TIME_SERIES_DISTRIBUTED_H
