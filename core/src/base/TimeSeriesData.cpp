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

bool TimeSeriesDataRegular::IsValid() const {
    // Check that values have been set
    if (_values.empty()) {
        wxLogError(_("TimeSeriesDataRegular: No values set."));
        return false;
    }

    // Check that start is before end
    if (_start > _end) {
        wxLogError(_("TimeSeriesDataRegular: Start date (%f) is after end date (%f)."), _start, _end);
        return false;
    }

    // Check that time step is positive
    if (_timeStep <= 0) {
        wxLogError(_("TimeSeriesDataRegular: Time step must be positive."));
        return false;
    }

    return true;
}

void TimeSeriesDataRegular::Validate() const {
    if (!IsValid()) {
        wxString msg = wxString::Format(
            _("TimeSeriesDataRegular validation failed. Start: %f, End: %f, TimeStep: %d, ValueCount: %d"), _start,
            _end, _timeStep, static_cast<int>(_values.size()));
        throw ModelConfigError(msg);
    }
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

bool TimeSeriesDataIrregular::IsValid() const {
    // Check that dates have been set
    if (_dates.empty()) {
        wxLogError(_("TimeSeriesDataIrregular: No dates set."));
        return false;
    }

    // Check that values have been set
    if (_values.empty()) {
        wxLogError(_("TimeSeriesDataIrregular: No values set."));
        return false;
    }

    // Check that dates and values match
    if (_dates.size() != _values.size()) {
        wxLogError(_("TimeSeriesDataIrregular: Date count (%d) does not match value count (%d)."),
                   static_cast<int>(_dates.size()), static_cast<int>(_values.size()));
        return false;
    }

    // Check that dates are sorted
    for (size_t i = 1; i < _dates.size(); ++i) {
        if (_dates[i] <= _dates[i - 1]) {
            wxLogError(_("TimeSeriesDataIrregular: Dates are not sorted (date[%d]=%f <= date[%d]=%f)."),
                       static_cast<int>(i), _dates[i], static_cast<int>(i - 1), _dates[i - 1]);
            return false;
        }
    }

    return true;
}

void TimeSeriesDataIrregular::Validate() const {
    if (!IsValid()) {
        wxString msg = wxString::Format(_("TimeSeriesDataIrregular validation failed. DateCount: %d, ValueCount: %d"),
                                        static_cast<int>(_dates.size()), static_cast<int>(_values.size()));
        throw ModelConfigError(msg);
    }
}
