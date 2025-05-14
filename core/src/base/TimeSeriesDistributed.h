#ifndef HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H
#define HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H

#include "Includes.h"
#include "TimeSeries.h"

class TimeSeriesDistributed : public TimeSeries {
  public:
    TimeSeriesDistributed(VariableType type);

    ~TimeSeriesDistributed() override;

    /**
     * Add data to the time series.
     *
     * @param data pointer to the time series data.
     * @param unitId ID of the unit.
     */
    void AddData(TimeSeriesData* data, int unitId);

    /**
     * @copydoc TimeSeries::SetCursorToDate()
     */
    bool SetCursorToDate(double date) override;

    /**
     * @copydoc TimeSeries::AdvanceOneTimeStep()
     */
    bool AdvanceOneTimeStep() override;

    /**
     * @copydoc TimeSeries::IsDistributed()
     */
    bool IsDistributed() override {
        return true;
    }

    /**
     * @copydoc TimeSeries::GetStart()
     */
    double GetStart() override;

    /**
     * @copydoc TimeSeries::GetEnd()
     */
    double GetEnd() override;

    /**
     * @copydoc TimeSeries::GetTotal()
     */
    double GetTotal(const SettingsBasin* basinSettings) override;

    /**
     * @copydoc TimeSeries::GetDataPointer()
     */
    TimeSeriesData* GetDataPointer(int unitId) override;

  protected:
    vecInt m_unitIds;
    vector<TimeSeriesData*> m_data;
};

#endif  // HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H
