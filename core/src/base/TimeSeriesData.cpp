#include "TimeSeriesData.h"

/*
 * TimeSeriesData
 */

TimeSeriesData::TimeSeriesData()
    : m_cursor(0) {}

bool TimeSeriesData::SetValues(const vecDouble& values) {
    m_values = values;
    return true;
}

double TimeSeriesData::GetValueFor(double) {
    throw NotImplemented();
}

double TimeSeriesData::GetCurrentValue() {
    throw NotImplemented();
}

double TimeSeriesData::GetSum() {
    throw NotImplemented();
}

/*
 * TimeSeriesDataRegular
 */

TimeSeriesDataRegular::TimeSeriesDataRegular(double start, double end, int timeStep, TimeUnit timeStepUnit)
    : TimeSeriesData(),
      m_start(start),
      m_end(end),
      m_timeStep(timeStep),
      m_timeStepUnit(timeStepUnit) {}

bool TimeSeriesDataRegular::SetValues(const vecDouble& values) {
    double calcEnd = IncrementDateBy(m_start, m_timeStep * static_cast<int>(values.size() - 1), m_timeStepUnit);
    if (calcEnd != m_end) {
        wxLogError(_("The size of the time series data does not match the time properties."));
        wxLogError(_("End of the data (%d) != end of the dates (%d)."), calcEnd, m_end);
        return false;
    }

    m_values = values;
    return true;
}

double TimeSeriesDataRegular::GetValueFor(double date) {
    SetCursorToDate(date);
    return m_values[m_cursor];
}

double TimeSeriesDataRegular::GetCurrentValue() {
    wxASSERT(m_values.size() > m_cursor);
    return m_values[m_cursor];
}

double TimeSeriesDataRegular::GetSum() {
    double sum = 0;
    for (const auto& value : m_values) sum += value;

    return sum;
}

bool TimeSeriesDataRegular::SetCursorToDate(double date) {
    if (date < m_start) {
        wxLogError(_("The desired date is before the data starting date."));
        return false;
    }
    if (date > m_end) {
        wxLogError(_("The desired date is after the data ending date."));
        return false;
    }

    double dt = date - m_start;

    switch (m_timeStepUnit) {
        case Day:
            m_cursor = static_cast<int>(dt);
            break;
        case Hour:
            m_cursor = static_cast<int>(dt) * 24;
            break;
        case Minute:
            m_cursor = static_cast<int>(dt) * 1440;
            break;
        default:
            throw NotImplemented();
    }

    return true;
}

bool TimeSeriesDataRegular::AdvanceOneTimeStep() {
    if (m_cursor >= m_values.size()) {
        wxLogError(_("The desired date is after the data ending date."));
        return false;
    }
    m_cursor++;

    return true;
}

double TimeSeriesDataRegular::GetStart() {
    return m_start;
}

double TimeSeriesDataRegular::GetEnd() {
    return m_end;
}

/*
 * TimeSeriesDataIrregular
 */

TimeSeriesDataIrregular::TimeSeriesDataIrregular(vecDouble& dates)
    : TimeSeriesData(),
      m_dates(dates) {}

bool TimeSeriesDataIrregular::SetValues(const vecDouble& values) {
    if (m_dates.size() != values.size()) {
        wxLogError(_("The size of the time series data does not match the dates array."));
        return false;
    }

    m_values = values;
    return true;
}

double TimeSeriesDataIrregular::GetValueFor(double) {
    throw NotImplemented();
}

double TimeSeriesDataIrregular::GetCurrentValue() {
    wxASSERT(m_values.size() > m_cursor);
    return m_values[m_cursor];
}

double TimeSeriesDataIrregular::GetSum() {
    throw NotImplemented();
}

bool TimeSeriesDataIrregular::SetCursorToDate(double) {
    throw NotImplemented();
}

bool TimeSeriesDataIrregular::AdvanceOneTimeStep() {
    throw NotImplemented();
}

double TimeSeriesDataIrregular::GetStart() {
    wxASSERT(!m_dates.empty());
    return m_dates[0];
}

double TimeSeriesDataIrregular::GetEnd() {
    wxASSERT(!m_dates.empty());
    return m_dates[m_dates.size() - 1];
}
