#ifndef HYDROBRICKS_TIME_SERIES_DATA_H
#define HYDROBRICKS_TIME_SERIES_DATA_H

#include "Includes.h"

class TimeSeriesData : public wxObject {
  public:
    TimeSeriesData();

    ~TimeSeriesData() override = default;

    /**
     * Set the values of the time series data.
     *
     * @param values vector of values to set.
     * @return true if the values were successfully set.
     */
    virtual bool SetValues(const vecDouble& values);

    /**
     * Get the value for the provided date.
     *
     * @param date date to get the value for.
     * @return the value for the provided date.
     */
    virtual double GetValueFor(double date);

    /**
     * Get the current value.
     *
     * @return the current value.
     */
    virtual double GetCurrentValue();

    /**
     * Get the sum of the values.
     *
     * @return the sum of the values.
     */
    virtual double GetSum();

    /**
     * Set the cursor to the provided date.
     *
     * @param date date to set the cursor to.
     * @return true if the cursor was successfully set to the provided date.
     */
    virtual bool SetCursorToDate(double date) = 0;

    /**
     * Advance the cursor to the next time step.
     *
     * @return true if the cursor was successfully advanced to the next time step.
     */
    virtual bool AdvanceOneTimeStep() = 0;

    /**
     * Get the start date of the time series data.
     *
     * @return the start date of the time series data.
     */
    virtual double GetStart() = 0;

    /**
     * Get the end date of the time series data.
     *
     * @return the end date of the time series data.
     */
    virtual double GetEnd() = 0;

  protected:
    vecDouble m_values;
    int m_cursor;
};

class TimeSeriesDataRegular : public TimeSeriesData {
  public:
    TimeSeriesDataRegular(double start, double end, int timeStep, TimeUnit timeStepUnit);

    ~TimeSeriesDataRegular() override = default;

    /**
    * @copydoc TimeSeriesData::SetValues()
    */
    bool SetValues(const vecDouble& values) override;

    /**
     * @copydoc TimeSeriesData::GetValueFor()
     */
    double GetValueFor(double date) override;

    /**
     * @copydoc TimeSeriesData::GetCurrentValue()
     */
    double GetCurrentValue() override;

    /**
     * @copydoc TimeSeriesData::GetSum()
     */
    double GetSum() override;

    /**
     * @copydoc TimeSeriesData::SetCursorToDate()
     */
    bool SetCursorToDate(double date) override;

    /**
     * @copydoc TimeSeriesData::AdvanceOneTimeStep()
     */
    bool AdvanceOneTimeStep() override;

    /**
     * @copydoc TimeSeriesData::GetStart()
     */
    double GetStart() override;

    /**
     * @copydoc TimeSeriesData::GetEnd()
     */
    double GetEnd() override;

  protected:
    double m_start;
    double m_end;
    int m_timeStep;
    TimeUnit m_timeStepUnit;
};

class TimeSeriesDataIrregular : public TimeSeriesData {
  public:
    explicit TimeSeriesDataIrregular(vecDouble& dates);

    ~TimeSeriesDataIrregular() override = default;

    /**
     * @copydoc TimeSeriesData::SetValues()
     */
    bool SetValues(const vecDouble& values) override;

    /**
     * @copydoc TimeSeriesData::GetValueFor()
     */
    double GetValueFor(double date) override;

    /**
     * @copydoc TimeSeriesData::GetCurrentValue()
     */
    double GetCurrentValue() override;

    /**
     * @copydoc TimeSeriesData::GetSum()
     */
    double GetSum() override;

    /**
     * @copydoc TimeSeriesData::SetCursorToDate()
     */
    bool SetCursorToDate(double date) override;

    /**
     * @copydoc TimeSeriesData::AdvanceOneTimeStep()
     */
    bool AdvanceOneTimeStep() override;

    /**
     * @copydoc TimeSeriesData::GetStart()
     */
    double GetStart() override;

    /**
     * @copydoc TimeSeriesData::GetEnd()
     */
    double GetEnd() override;

  protected:
    vecDouble m_dates;
};

#endif  // HYDROBRICKS_TIME_SERIES_DATA_H
