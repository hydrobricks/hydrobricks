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
        _timeStepUnit = Day;
    } else if (settings.timeStepUnit == "hour") {
        _timeStepUnit = Hour;
    } else if (settings.timeStepUnit == "minute") {
        _timeStepUnit = Minute;
    } else {
        throw InvalidArgument(_("Time step unit unrecognized or not implemented."));
    }

    UpdateTimeStepInDays();
}

void TimeMachine::Reset() {
    _date = _start;
}

bool TimeMachine::IsOver() {
    return _date > _end;
}

void TimeMachine::IncrementTime() {
    wxASSERT(_timeStepInDays > 0);
    _date += _timeStepInDays;

    if (_parametersUpdater) {
        _parametersUpdater->DateUpdate(_date);
    }
    if (_actionsManager) {
        _actionsManager->DateUpdate(_date);
    }
}

int TimeMachine::GetTimeStepsNb() {
    wxASSERT(_timeStepInDays > 0);
    return static_cast<int>(1 + (_end - _start) / _timeStepInDays);
}

void TimeMachine::UpdateTimeStepInDays() {
    switch (_timeStepUnit) {
        case Variable:
            throw NotImplemented();
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
