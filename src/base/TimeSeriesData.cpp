
#include "TimeSeriesData.h"

/*
 * TimeSeriesData
 */

TimeSeriesData::TimeSeriesData() {}

bool TimeSeriesData::SetValues(std::vector<double> &values) {
    m_values = values;
    return true;
}

double TimeSeriesData::GetValueFor(const wxDateTime &date) {
    wxFAIL;
    return 0;
}


/*
 * TimeSeriesDataRegular
 */

TimeSeriesDataRegular::TimeSeriesDataRegular(const wxDateTime &start, const wxDateTime &end, int timeStep,
                                             TimeUnit timeStepUnit)
    : m_start(start),
      m_end(end),
      m_timeStep(timeStep),
      m_timeStepUnit(timeStepUnit)
{}

bool TimeSeriesDataRegular::SetValues(std::vector<double> &values) {
    wxDateTime calcEnd = IncrementDateBy(m_start, m_timeStep * int(values.size() - 1), m_timeStepUnit);
    if (!calcEnd.IsEqualTo(m_end)) {
        wxLogError(_("The size of the time series data does not match the time properties."));
        wxLogError(_("End of the data (%s) != end of the dates (%s)."), calcEnd.FormatISOCombined(),
                   m_end.FormatISOCombined());
        return false;
    }

    m_values = values;
    return true;
}

double TimeSeriesDataRegular::GetValueFor(const wxDateTime &date) {
    wxFAIL;
    return 0;
}


/*
 * TimeSeriesDataIrregular
 */

TimeSeriesDataIrregular::TimeSeriesDataIrregular(std::vector<double> &dates)
    : m_dates(dates)
{}

bool TimeSeriesDataIrregular::SetValues(std::vector<double> &values) {
    if (m_dates.size() != values.size()) {
        wxLogError(_("The size of the time series data does not match the dates array."));
        return false;
    }

    m_values = values;
    return true;
}

double TimeSeriesDataIrregular::GetValueFor(const wxDateTime &date) {
    wxFAIL;
    return 0;
}

