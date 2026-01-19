#ifndef HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H
#define HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H

#include <memory>

#include "Includes.h"
#include "TimeSeries.h"

class TimeSeriesDistributed : public TimeSeries {
  public:
    TimeSeriesDistributed(VariableType type);

    ~TimeSeriesDistributed() override;

    /**
     * Add data to the time series.
     *
     * @param data pointer to the time series data (ownership transferred).
     * @param unitId ID of the unit.
     */
    void AddData(std::unique_ptr<TimeSeriesData> data, int unitId);

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
        return true;
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
    vecInt _unitIds;
    std::vector<std::unique_ptr<TimeSeriesData>> _data;  // owning
};

#endif  // HYDROBRICKS_TIME_SERIES_DISTRIBUTED_H
