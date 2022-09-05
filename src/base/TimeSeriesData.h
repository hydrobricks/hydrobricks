#ifndef HYDROBRICKS_TIME_SERIES_DATA_H
#define HYDROBRICKS_TIME_SERIES_DATA_H

#include "Includes.h"

class TimeSeriesData : public wxObject {
  public:
    TimeSeriesData();

    ~TimeSeriesData() override = default;

    virtual bool SetValues(const vecDouble &values);

    virtual double GetValueFor(double date);

    virtual double GetCurrentValue();

    virtual bool SetCursorToDate(double date) = 0;

    virtual bool AdvanceOneTimeStep() = 0;

    virtual double GetStart() = 0;

    virtual double GetEnd() = 0;

  protected:
    vecDouble m_values;
    int m_cursor;

  private:
};


class TimeSeriesDataRegular : public TimeSeriesData {
  public:
    TimeSeriesDataRegular(double start, double end, int timeStep, TimeUnit timeStepUnit);

    ~TimeSeriesDataRegular() override = default;

    bool SetValues(const vecDouble &values) override;

    double GetValueFor(double date) override;

    double GetCurrentValue() override;

    bool SetCursorToDate(double date) override;

    bool AdvanceOneTimeStep() override;

    double GetStart() override;

    double GetEnd() override;

  protected:
    double m_start;
    double m_end;
    int m_timeStep;
    TimeUnit m_timeStepUnit;

  private:
};


class TimeSeriesDataIrregular : public TimeSeriesData {
  public:
    explicit TimeSeriesDataIrregular(vecDouble &dates);

    ~TimeSeriesDataIrregular() override = default;

    bool SetValues(const vecDouble &values) override;

    double GetValueFor(double date) override;

    double GetCurrentValue() override;

    bool SetCursorToDate(double date) override;

    bool AdvanceOneTimeStep() override;

    double GetStart() override;

    double GetEnd() override;

  protected:
    vecDouble m_dates;

  private:
};

#endif  // HYDROBRICKS_TIME_SERIES_DATA_H
