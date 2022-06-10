#include "TimeSeriesData.h"

/*
 * TimeSeriesData
 */

TimeSeriesData::TimeSeriesData()
    : m_cursor(0)
{}

bool TimeSeriesData::SetValues(const vecDouble &values) {
    m_values = values;
    return true;
}

double TimeSeriesData::GetValueFor(const wxDateTime &) {
    throw NotImplemented();
}

double TimeSeriesData::GetCurrentValue() {
    throw NotImplemented();
}


/*
 * TimeSeriesDataRegular
 */

TimeSeriesDataRegular::TimeSeriesDataRegular(const wxDateTime &start, const wxDateTime &end, int timeStep,
                                             TimeUnit timeStepUnit)
    : TimeSeriesData(),
      m_start(start),
      m_end(end),
      m_timeStep(timeStep),
      m_timeStepUnit(timeStepUnit)
{}

bool TimeSeriesDataRegular::SetValues(const vecDouble &values) {
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
    SetCursorToDate(date);
    return m_values[m_cursor];
}

double TimeSeriesDataRegular::GetCurrentValue() {
    wxASSERT(m_values.size() > m_cursor);
    return m_values[m_cursor];
}

bool TimeSeriesDataRegular::SetCursorToDate(const wxDateTime &dateTime) {
    if (dateTime.IsEarlierThan(m_start)) {
        wxLogError(_("The desired date is before the data starting date."));
        return false;
    }
    if (dateTime.IsLaterThan(m_end)) {
        wxLogError(_("The desired date is after the data ending date."));
        return false;
    }

    wxTimeSpan dt = dateTime.Subtract(m_start);

    switch (m_timeStepUnit) {
        case Day:
            m_cursor = dt.GetDays();
            break;
        case Hour:
            m_cursor = dt.GetHours();
            break;
        case Minute:
            m_cursor = dt.GetMinutes();
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

wxDateTime TimeSeriesDataRegular::GetStart() {
    return m_start;
}

wxDateTime TimeSeriesDataRegular::GetEnd() {
    return m_end;
}

/*
 * TimeSeriesDataIrregular
 */

TimeSeriesDataIrregular::TimeSeriesDataIrregular(vecDouble &dates)
    : TimeSeriesData(),
      m_dates(dates)
{}

bool TimeSeriesDataIrregular::SetValues(const vecDouble &values) {
    if (m_dates.size() != values.size()) {
        wxLogError(_("The size of the time series data does not match the dates array."));
        return false;
    }

    m_values = values;
    return true;
}

double TimeSeriesDataIrregular::GetValueFor(const wxDateTime &) {
    throw NotImplemented();
}

double TimeSeriesDataIrregular::GetCurrentValue() {
    wxASSERT(m_values.size() > m_cursor);
    return m_values[m_cursor];
}

bool TimeSeriesDataIrregular::SetCursorToDate(const wxDateTime &) {
    throw NotImplemented();
}

bool TimeSeriesDataIrregular::AdvanceOneTimeStep() {
    throw NotImplemented();
}

wxDateTime TimeSeriesDataIrregular::GetStart() {
    wxASSERT(!m_dates.empty());
    return m_dates[0];
}

wxDateTime TimeSeriesDataIrregular::GetEnd() {
    wxASSERT(!m_dates.empty());
    return m_dates[m_dates.size()-1];
}