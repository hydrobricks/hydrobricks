#ifndef HYDROBRICKS_TIME_SERIES_H
#define HYDROBRICKS_TIME_SERIES_H

#include "Includes.h"
#include "TimeSeriesData.h"

class TimeSeries : public wxObject {
  public:
    TimeSeries(VariableType type);

    ~TimeSeries() override = default;

    VariableType GetVariableType() {
        return m_type;
    }

  protected:
    VariableType m_type;

  private:
};

#endif  // HYDROBRICKS_TIME_SERIES_H
