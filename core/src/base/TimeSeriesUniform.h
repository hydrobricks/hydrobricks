#ifndef HYDROBRICKS_TIME_SERIES_UNIFORM_H
#define HYDROBRICKS_TIME_SERIES_UNIFORM_H

#include "Includes.h"
#include "TimeSeries.h"

class TimeSeriesUniform : public TimeSeries {
  public:
    TimeSeriesUniform(VariableType type);

    ~TimeSeriesUniform() override;

    /**
     * Set the time series data.
     *
     * @param data pointer to the time series data.
     */
    void SetData(TimeSeriesData* data) {
        wxASSERT(data);
        _data = data;
    }

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
        return false;
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
    TimeSeriesData* _data;
};

#endif  // HYDROBRICKS_TIME_SERIES_UNIFORM_H
