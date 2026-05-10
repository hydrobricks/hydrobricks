#include "TimeMachine.h"

TimeMachine::TimeMachine()
    : _date(0),
      _start(0),
      _end(0),
      _timeStep(0),
      _timeStepUnit(TimeUnit::Day),
      _timeStepInDays(0),
      _parametersUpdater(nullptr),
      _actionsManager(nullptr) {}

void TimeMachine::Initialize(double start, double end, int timeStep, TimeUnit timeStepUnit) {
    _date = start;
    _start = start;
    _end = end;
    _timeStep = timeStep;
    _timeStepUnit = timeStepUnit;
    UpdateTimeStepInDays();
}

void TimeMachine::Initialize(const TimerSettings& settings) {
    _start = ParseDate(settings.start, guess);
    _end = ParseDate(settings.end, guess);
    _date = _start;
    _timeStep = settings.timeStep;

    if (settings.timeStepUnit == "day") {
        _timeStepUnit = TimeUnit::Day;
    } else if (settings.timeStepUnit == "hour") {
        _timeStepUnit = TimeUnit::Hour;
    } else if (settings.timeStepUnit == "minute") {
        _timeStepUnit = TimeUnit::Minute;
    } else {
        throw InputError("Time step unit unrecognized or not implemented.");
    }

    UpdateTimeStepInDays();
}

void TimeMachine::Reset() {
    _date = _start;
}

bool TimeMachine::IsOver() const {
    return _date > _end;
}

void TimeMachine::IncrementTime() {
    assert(_timeStepInDays > 0);
    _date += _timeStepInDays;

    if (_parametersUpdater) {
        _parametersUpdater->DateUpdate(_date);
    }
    if (_actionsManager) {
        _actionsManager->DateUpdate(_date);
    }
}

int TimeMachine::GetTimeStepCount() const {
    assert(_timeStepInDays > 0);
    return static_cast<int>(1 + (_end - _start) / _timeStepInDays);
}

void TimeMachine::UpdateTimeStepInDays() {
    switch (_timeStepUnit) {
        case TimeUnit::Variable:
            throw NotImplemented("TimeMachine::UpdateTimeStepInDays - Variable time step unit not yet supported");
        case TimeUnit::Week:
            _timeStepInDays = _timeStep * 7;
            break;
        case TimeUnit::Day:
            _timeStepInDays = _timeStep;
            break;
        case TimeUnit::Hour:
            _timeStepInDays = _timeStep / 24.0;
            break;
        case TimeUnit::Minute:
            _timeStepInDays = _timeStep / 1440.0;
            break;
        default:
            LogError("The provided time step unit is not allowed.");
    }
}

int TimeMachine::GetCurrentDayOfYear() const {
    if (_date <= 0) return 1;
    Time ts = GetTimeStructFromMJD(_date);
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
        LogError("TimeMachine: Start or end date not properly set.");
        return false;
    }

    // Check that start is before end
    if (_start > _end) {
        LogError("TimeMachine: Start date ({}) is after end date ({}).", _start, _end);
        return false;
    }

    // Check that time step is positive
    if (_timeStep <= 0) {
        LogError("TimeMachine: Time step must be positive.");
        return false;
    }

    // Check that time step in days is positive
    if (_timeStepInDays <= 0) {
        LogError("TimeMachine: Time step in days must be positive.");
        return false;
    }

    return true;
}

void TimeMachine::Validate() const {
    if (!IsValid()) {
        string msg = std::format("TimeMachine validation failed. Start: {}, End: {}, TimeStep: {}, TimeStepInDays: {}",
                                 _start, _end, _timeStep, _timeStepInDays);
        throw ModelConfigError(msg);
    }
}
