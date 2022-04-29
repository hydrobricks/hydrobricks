#ifndef HYDROBRICKS_TIME_SERIES_UNIFORM_H
#define HYDROBRICKS_TIME_SERIES_UNIFORM_H

#include "Includes.h"
#include "TimeSeries.h"

class TimeSeriesUniform : public TimeSeries {
  public:
    TimeSeriesUniform(VariableType type);

    ~TimeSeriesUniform() override;

    void SetData(TimeSeriesData* data) {
        wxASSERT(data);
        m_data = data;
    }

    bool SetCursorToDate(const wxDateTime &dateTime) override;

    bool AdvanceOneTimeStep() override;

    bool IsDistributed() override {
        return false;
    }

    TimeSeriesData* GetDataPointer(int unitId) override;

  protected:
    TimeSeriesData* m_data;

  private:
};

#endif  // HYDROBRICKS_TIME_SERIES_UNIFORM_H
