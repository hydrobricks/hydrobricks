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
            throw NotImplemented();
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

double TimeSeriesDataRegular::GetStart() {
    return _start;
}

double TimeSeriesDataRegular::GetEnd() {
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
    throw NotImplemented();
}

double TimeSeriesDataIrregular::GetCurrentValue() {
    wxASSERT(_values.size() > _cursor);
    return _values[_cursor];
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
    wxASSERT(!_dates.empty());
    return _dates[0];
}

double TimeSeriesDataIrregular::GetEnd() {
    wxASSERT(!_dates.empty());
    return _dates[_dates.size() - 1];
}
