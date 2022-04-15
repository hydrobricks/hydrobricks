
#ifndef HYDROBRICKS_TIME_SERIES_DATA_H
#define HYDROBRICKS_TIME_SERIES_DATA_H

#include "Includes.h"

class TimeSeriesData : public wxObject {
  public:
    TimeSeriesData();

    ~TimeSeriesData() override = default;

    virtual bool SetValues(std::vector<double> &values);

    virtual double GetValueFor(const wxDateTime &date);

  protected:
    std::vector<double> m_values;
    size_t m_index;

  private:
};


class TimeSeriesDataRegular : public TimeSeriesData {
  public:
    TimeSeriesDataRegular(const wxDateTime &start, const wxDateTime &end, int timeStep, TimeUnit timeStepUnit);

    ~TimeSeriesDataRegular() override = default;

    bool SetValues(std::vector<double> &values) override;

    double GetValueFor(const wxDateTime &date) override;

  protected:
    wxDateTime m_start;
    wxDateTime m_end;
    int m_timeStep;
    TimeUnit m_timeStepUnit;

  private:
};


class TimeSeriesDataIrregular : public TimeSeriesData {
  public:
    TimeSeriesDataIrregular(std::vector<double> &dates);

    ~TimeSeriesDataIrregular() override = default;

    bool SetValues(std::vector<double> &values) override;

    double GetValueFor(const wxDateTime &date) override;

  protected:
    std::vector<double> m_dates;

  private:
};

#endif  // HYDROBRICKS_TIME_SERIES_DATA_H
