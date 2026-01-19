#ifndef HYDROBRICKS_TIME_SERIES_UNIFORM_H
#define HYDROBRICKS_TIME_SERIES_UNIFORM_H

#include <memory>

#include "Includes.h"
#include "TimeSeries.h"

class TimeSeriesUniform : public TimeSeries {
  public:
    TimeSeriesUniform(VariableType type);

    ~TimeSeriesUniform() override;

    /**
     * Set the time series data.
     *
     * @param data pointer to the time series data (ownership transferred).
     */
    void SetData(std::unique_ptr<TimeSeriesData> data) {
        wxASSERT(data);
        _data = std::move(data);
    }

    /**
     * @copydoc TimeSeries::SetCursorToDate()
     */
    [[nodiscard]] bool SetCursorToDate(double date) override;

    /**
     * @copydoc TimeSeries::AdvanceOneTimeStep()
     */
    [[nodiscard]] bool AdvanceOneTimeStep() override;

    /**
     * @copydoc TimeSeries::IsDistributed()
     */
    bool IsDistributed() const override {
        return false;
    }

    /**
     * @copydoc TimeSeries::GetStart()
     */
    double GetStart() const override;

    /**
     * @copydoc TimeSeries::GetEnd()
     */
    double GetEnd() const override;

    /**
     * @copydoc TimeSeries::GetTotal()
     */
    double GetTotal(const SettingsBasin* basinSettings) override;

    /**
     * @copydoc TimeSeries::GetDataPointer()
     */
    TimeSeriesData* GetDataPointer(int unitId) override;

  protected:
    std::unique_ptr<TimeSeriesData> _data;  // owning
};

#endif  // HYDROBRICKS_TIME_SERIES_UNIFORM_H
