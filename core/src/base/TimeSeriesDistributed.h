#ifndef HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H
#define HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H

#include "Includes.h"
#include "TimeSeries.h"

class TimeSeriesDistributed : public TimeSeries {
  public:
    TimeSeriesDistributed(VariableType type);

    ~TimeSeriesDistributed() override;

    void AddData(TimeSeriesData* data, int unitId);

    bool SetCursorToDate(double date) override;

    bool AdvanceOneTimeStep() override;

    bool IsDistributed() override {
        return true;
    }

    double GetStart() override;

    double GetEnd() override;

    double GetTotal(const SettingsBasin* basinSettings) override;

    TimeSeriesData* GetDataPointer(int unitId) override;

  protected:
    vecInt m_unitIds;
    vector<TimeSeriesData*> m_data;

  private:
};

#endif  // HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H
