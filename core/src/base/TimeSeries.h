#ifndef HYDROBRICKS_TIME_SERIES_H
#define HYDROBRICKS_TIME_SERIES_H

#include "Includes.h"
#include "SettingsBasin.h"
#include "TimeSeriesData.h"

class TimeSeries : public wxObject {
  public:
    explicit TimeSeries(VariableType type);

    ~TimeSeries() override = default;

    /**
     * Parse the time series netCDF file.
     *
     * @param path path to the netCDF file.
     * @param vecTimeSeries vector to store the parsed time series.
     */
    static bool Parse(const string& path, vector<TimeSeries*>& vecTimeSeries);

    /**
     * Create a time series from the provided data.
     *
     * @param varName name of the variable.
     * @param time time data.
     * @param ids unit IDs.
     * @param data time series data.
     * @return pointer to the created time series.
     */
    static TimeSeries* Create(const string& varName, const axd& time, const axi& ids, const axxd& data);

    /**
     * Set the internal cursor to the provided date.
     *
     * @param date date to set the cursor to.
     * @return true if the cursor was successfully set to the provided date.
     */
    virtual bool SetCursorToDate(double date) = 0;

    /**
     * Advance the internal cursor to the next time step.
     *
     * @return true if the cursor was successfully advanced to the next time step.
     */
    virtual bool AdvanceOneTimeStep() = 0;

    /**
     * Check if the time series is distributed.
     *
     * @return true if the time series is distributed.
     */
    virtual bool IsDistributed() = 0;

    /**
     * Get the time start of the time series.
     *
     * @return the time start of the time series.
     */
    virtual double GetStart() = 0;

    /**
     * Get the time end of the time series.
     *
     * @return the time end of the time series.
     */
    virtual double GetEnd() = 0;

    /**
     * Get the sum of the time series data for the provided basin settings.
     *
     * @param basinSettings settings of the basin.
     * @return the sum of the time series data.
     */
    virtual double GetTotal(const SettingsBasin* basinSettings) = 0;

    /**
     * Get the data pointer for the provided unit ID.
     *
     * @param unitId ID of the unit.
     * @return pointer to the time series data for the provided unit ID.
     */
    virtual TimeSeriesData* GetDataPointer(int unitId) = 0;

    /**
     * Get the variable type of the time series.
     *
     * @return the variable type of the time series.
     */
    VariableType GetVariableType() {
        return m_type;
    }

  protected:
    VariableType m_type;

  private:
    /**
     * Extract the time step and time unit from the provided time step data.
     *
     * @param timeStepData time step data.
     * @param timeStep reference to store the extracted time step.
     * @param timeUnit reference to store the extracted time unit.
     */
    static void ExtractTimeStep(double timeStepData, int& timeStep, TimeUnit& timeUnit);

    /**
     * Match the variable name to the corresponding variable type.
     *
     * @param varName name of the variable.
     * @return the matched variable type.
     */
    static VariableType MatchVariableType(const string& varName);
};

#endif  // HYDROBRICKS_TIME_SERIES_H
