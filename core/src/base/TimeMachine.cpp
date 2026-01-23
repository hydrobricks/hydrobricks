#include "TimeMachine.h"

TimeMachine::TimeMachine()
    : _date(0),
      _start(0),
      _end(0),
      _timeStep(0),
      _timeStepUnit(Day),
      _timeStepInDays(0),
      _parametersUpdater(nullptr),
      _actionsManager(nullptr) {}

double TimeMachine::_currentDateStatic = 0.0;

void TimeMachine::Initialize(double start, double end, int timeStep, TimeUnit timeStepUnit) {
    _date = start;
    _start = start;
    _end = end;
    _timeStep = timeStep;
    _timeStepUnit = timeStepUnit;
    _currentDateStatic = _date;  // Sync static date
    UpdateTimeStepInDays();
}

void TimeMachine::Initialize(const TimerSettings& settings) {
    _start = ParseDate(settings.start, guess);
    _end = ParseDate(settings.end, guess);
    _date = _start;
    _timeStep = settings.timeStep;

    if (settings.timeStepUnit == "day") {
        _timeStepUnit = Day;
    } else if (settings.timeStepUnit == "hour") {
        _timeStepUnit = Hour;
    } else if (settings.timeStepUnit == "minute") {
        _timeStepUnit = Minute;
    } else {
        throw InputError(_("Time step unit unrecognized or not implemented."));
    }

    UpdateTimeStepInDays();
    _currentDateStatic = _date;  // Sync static date
}

void TimeMachine::Reset() {
    _date = _start;
    _currentDateStatic = _date;  // Sync static date
}

bool TimeMachine::IsOver() const {
    return _date > _end;
}

void TimeMachine::IncrementTime() {
    wxASSERT(_timeStepInDays > 0);
    _date += _timeStepInDays;
    _currentDateStatic = _date;  // Sync static date

    if (_parametersUpdater) {
        _parametersUpdater->DateUpdate(_date);
    }
    if (_actionsManager) {
        _actionsManager->DateUpdate(_date);
    }
}

int TimeMachine::GetTimeStepCount() const {
    wxASSERT(_timeStepInDays > 0);
    return static_cast<int>(1 + (_end - _start) / _timeStepInDays);
}

void TimeMachine::UpdateTimeStepInDays() {
    switch (_timeStepUnit) {
        case Variable:
            throw NotImplemented("TimeMachine::UpdateTimeStepInDays - Variable time step unit not yet supported");
        case Week:
            _timeStepInDays = _timeStep * 7;
            break;
        case Day:
            _timeStepInDays = _timeStep;
            break;
        case Hour:
            _timeStepInDays = _timeStep / 24.0;
            break;
        case Minute:
            _timeStepInDays = _timeStep / 1440.0;
            break;
        default:
            wxLogError(_("The provided time step unit is not allowed."));
    }
}

int TimeMachine::GetCurrentDayOfYear() {
    if (_currentDateStatic <= 0) return 1;
    Time ts = GetTimeStructFromMJD(_currentDateStatic);
    static const int cumMonthDays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int monthIndex = ts.month - 1;  // 0-based
    if (monthIndex < 0 || monthIndex > 11) return 1;
    bool leap = (ts.year % 4 == 0 && (ts.year % 100 != 0 || ts.year % 400 == 0));
    int doy = cumMonthDays[monthIndex] + ts.day;
    if (leap && monthIndex > 1) doy += 1;

    return doy;
}

bool TimeMachine::IsValid() const {
    // Check that start and end dates are set
    if (_start <= 0 || _end <= 0) {
        wxLogError(_("TimeMachine: Start or end date not properly set."));
        return false;
    }

    // Check that start is before end
    if (_start > _end) {
        wxLogError(_("TimeMachine: Start date (%f) is after end date (%f)."), _start, _end);
        return false;
    }

    // Check that time step is positive
    if (_timeStep <= 0) {
        wxLogError(_("TimeMachine: Time step must be positive."));
        return false;
    }

    // Check that time step in days is positive
    if (_timeStepInDays <= 0) {
        wxLogError(_("TimeMachine: Time step in days must be positive."));
        return false;
    }

    return true;
}

void TimeMachine::Validate() const {
    if (!IsValid()) {
        wxString msg = wxString::Format(
            _("TimeMachine validation failed. Start: %f, End: %f, TimeStep: %d, TimeStepInDays: %f"),
            _start, _end, _timeStep, _timeStepInDays);
        throw ModelConfigError(msg);
    }
}


