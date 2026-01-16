#include "TimeSeriesData.h"

/*
 * TimeSeriesData
 */

TimeSeriesData::TimeSeriesData()
    : _cursor(0) {}

bool TimeSeriesData::SetValues(const vecDouble& values) {
    _values = values;
    return true;
}

double TimeSeriesData::GetValueFor(double) {
    throw NotImplemented("TimeSeriesData::GetValueFor - Not yet implemented");
}

double TimeSeriesData::GetCurrentValue() {
    throw NotImplemented("TimeSeriesData::GetCurrentValue - Not yet implemented");
}

double TimeSeriesData::GetSum() {
    throw NotImplemented("TimeSeriesData::GetSum - Not yet implemented");
}

/*
 * TimeSeriesDataRegular
 */

TimeSeriesDataRegular::TimeSeriesDataRegular(double start, double end, int timeStep, TimeUnit timeStepUnit)
    : TimeSeriesData(),
      _start(start),
      _end(end),
      _timeStep(timeStep),
      _timeStepUnit(timeStepUnit) {}

bool TimeSeriesDataRegular::SetValues(const vecDouble& values) {
    double calcEnd = IncrementDateBy(_start, _timeStep * static_cast<int>(values.size() - 1), _timeStepUnit);
    if (calcEnd != _end) {
        wxLogError(_("The size of the time series data does not match the time properties."));
        wxLogError(_("End of the data (%d) != end of the dates (%d)."), calcEnd, _end);
        return false;
    }

    _values = values;
    return true;
}

double TimeSeriesDataRegular::GetValueFor(double date) {
    SetCursorToDate(date);
    return _values[_cursor];
}

double TimeSeriesDataRegular::GetCurrentValue() {
    wxASSERT(_values.size() > _cursor);
    return _values[_cursor];
}

double TimeSeriesDataRegular::GetSum() {
    double sum = 0;
    for (const auto& value : _values) sum += value;

    return sum;
}

bool TimeSeriesDataRegular::SetCursorToDate(double date) {
    if (date < _start) {
        wxLogError(_("The desired date is before the data starting date."));
        return false;
    }
    if (date > _end) {
        wxLogError(_("The desired date is after the data ending date."));
        return false;
    }

    double dt = date - _start;

    switch (_timeStepUnit) {
        case Day:
            _cursor = static_cast<int>(dt);
            break;
        case Hour:
            _cursor = static_cast<int>(dt) * 24;
            break;
        case Minute:
            _cursor = static_cast<int>(dt) * 1440;
            break;
        default:
            throw NotImplemented(wxString::Format("TimeSeriesDataRegular::SetCursorToDate - Time unit %d not supported",
                                                   static_cast<int>(_timeStepUnit)));
    }

    return true;
}

bool TimeSeriesDataRegular::AdvanceOneTimeStep() {
    if (_cursor >= _values.size()) {
        wxLogError(_("The desired date is after the data ending date."));
        return false;
    }
    _cursor++;

    return true;
}

double TimeSeriesDataRegular::GetStart() const {
    return _start;
}

double TimeSeriesDataRegular::GetEnd() const {
    return _end;
}

/*
 * TimeSeriesDataIrregular
 */

TimeSeriesDataIrregular::TimeSeriesDataIrregular(vecDouble& dates)
    : TimeSeriesData(),
      _dates(dates) {}

bool TimeSeriesDataIrregular::SetValues(const vecDouble& values) {
    if (_dates.size() != values.size()) {
        wxLogError(_("The size of the time series data does not match the dates array."));
        return false;
    }

    _values = values;
    return true;
}

double TimeSeriesDataIrregular::GetValueFor(double) {
    throw NotImplemented("TimeSeriesDataIrregular::GetValueFor - Not yet implemented");
}

double TimeSeriesDataIrregular::GetCurrentValue() {
    wxASSERT(_values.size() > _cursor);
    return _values[_cursor];
}

double TimeSeriesDataIrregular::GetSum() {
    throw NotImplemented("TimeSeriesDataIrregular::GetSum - Not yet implemented");
}

bool TimeSeriesDataIrregular::SetCursorToDate(double) {
    throw NotImplemented("TimeSeriesDataIrregular::SetCursorToDate - Not yet implemented");
}

bool TimeSeriesDataIrregular::AdvanceOneTimeStep() {
    throw NotImplemented("TimeSeriesDataIrregular::AdvanceOneTimeStep - Not yet implemented");
}

double TimeSeriesDataIrregular::GetStart() const {
    wxASSERT(!_dates.empty());
    return _dates[0];
}

double TimeSeriesDataIrregular::GetEnd() const {
    wxASSERT(!_dates.empty());
    return _dates[_dates.size() - 1];
}
