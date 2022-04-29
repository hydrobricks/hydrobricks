#ifndef HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H
#define HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H

#include "Includes.h"
#include "TimeSeries.h"

class TimeSeriesDistributed : public TimeSeries {
  public:
    TimeSeriesDistributed(VariableType type);

    ~TimeSeriesDistributed() override;

    void AddData(TimeSeriesData* data, int unitId);

    bool SetCursorToDate(const wxDateTime &dateTime) override;

    bool AdvanceOneTimeStep() override;

    bool IsDistributed() override {
        return true;
    }

    TimeSeriesData* GetDataPointer(int unitId) override;

  protected:
    std::vector<int> m_unitIds;
    std::vector<TimeSeriesData*> m_data;

  private:
};

#endif  // HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H
