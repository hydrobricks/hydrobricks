#ifndef HYDROBRICKS_TIME_SERIES_DATA_H
#define HYDROBRICKS_TIME_SERIES_DATA_H

#include "Includes.h"

class TimeSeriesData : public wxObject {
  public:
    TimeSeriesData();

    ~TimeSeriesData() override = default;

    virtual bool SetValues(const vecDouble &values);

    virtual double GetValueFor(const wxDateTime &date);

    virtual double GetCurrentValue();

    virtual bool SetCursorToDate(const wxDateTime &dateTime) = 0;

    virtual bool AdvanceOneTimeStep() = 0;

  protected:
    vecDouble m_values;
    int m_cursor;

  private:
};


class TimeSeriesDataRegular : public TimeSeriesData {
  public:
    TimeSeriesDataRegular(const wxDateTime &start, const wxDateTime &end, int timeStep, TimeUnit timeStepUnit);

    ~TimeSeriesDataRegular() override = default;

    bool SetValues(const vecDouble &values) override;

    double GetValueFor(const wxDateTime &date) override;

    double GetCurrentValue() override;

    bool SetCursorToDate(const wxDateTime &dateTime) override;

    bool AdvanceOneTimeStep() override;

  protected:
    wxDateTime m_start;
    wxDateTime m_end;
    int m_timeStep;
    TimeUnit m_timeStepUnit;

  private:
};


class TimeSeriesDataIrregular : public TimeSeriesData {
  public:
    explicit TimeSeriesDataIrregular(vecDouble &dates);

    ~TimeSeriesDataIrregular() override = default;

    bool SetValues(const vecDouble &values) override;

    double GetValueFor(const wxDateTime &date) override;

    double GetCurrentValue() override;

    bool SetCursorToDate(const wxDateTime &dateTime) override;

    bool AdvanceOneTimeStep() override;

  protected:
    vecDouble m_dates;

  private:
};

#endif  // HYDROBRICKS_TIME_SERIES_DATA_H
