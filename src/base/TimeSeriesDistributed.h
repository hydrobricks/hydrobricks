
#ifndef HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H
#define HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H

#include "Includes.h"
#include "TimeSeries.h"

class TimeSeriesDistributed : public TimeSeries {
  public:
    TimeSeriesDistributed(VariableType type);

    ~TimeSeriesDistributed() override = default;

  protected:
    std::vector<TimeSeriesData*> m_data;

  private:
};

#endif  // HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H
