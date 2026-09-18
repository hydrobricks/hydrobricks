#include "ProcessRoutingDelay.h"

#include <cmath>
#include <numeric>

#include "Brick.h"
#include "WaterContainer.h"

ProcessRoutingDelay::ProcessRoutingDelay(WaterContainer* container)
    : ProcessOutflow(container),
      _delay(nullptr),
      _lastDelay(-1.0),
      _lastTimeStep(0.0),
      _previousContent(0.0),
      _processStorage(0.0) {}

void ProcessRoutingDelay::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("delay", 0.0f);
    if (modelSettings->LogAll()) {
        modelSettings->AddProcessLogging("internal_storage");
    }
}

bool ProcessRoutingDelay::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (_delay == nullptr) {
        LogError("Delay routing process: missing the 'delay' parameter.");
        return false;
    }

    return true;
}

void ProcessRoutingDelay::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _delay = GetParameterValuePointer(processSettings, "delay");
    _recomputeOrdinates();
}

void ProcessRoutingDelay::Reset() {
    ProcessOutflow::Reset();
    _recomputeOrdinates();
    std::fill(_schedule.begin(), _schedule.end(), 0.0);
    _previousContent = 0.0;
    _processStorage = 0.0;
}

double* ProcessRoutingDelay::GetValuePointer(std::string_view name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }
    if (name == "internal_storage") {
        return &_processStorage;
    }

    return nullptr;
}

const vecDouble& ProcessRoutingDelay::GetRates() {
    if (_delay == nullptr) {
        return StoreRates({0});
    }

    // Rebuild the ordinates when the delay changed (calibration loop) or when the step is
    // not the one they were built for (the structure is built before the timer is set).
    if (*_delay != _lastDelay || GetTimeStepInDays() != _lastTimeStep) {
        _recomputeOrdinates();
    }

    // What is scheduled for this step, plus the same-step share of the water arriving
    // now, both as rates. The arriving water is read as it enters during the solve: the
    // current rates of the upstream solver bricks and this step's amounts from forcing
    // and instantaneous inputs. With a zero delay the whole inflow leaves at the rate it
    // arrives, so the brick is transparent and the delay changes nothing.
    double timeStep = GetTimeStepInDays();
    double inRate = _container->SumIncomingChangeRates() + _container->SumIncomingAmounts() / timeStep;

    return StoreRates({std::max(0.0, _schedule[0]) / timeStep + _ordinates[0] * inRate});
}

void ProcessRoutingDelay::Finalize() {
    // Advance the delivery schedule once per step, from the amounts actually committed:
    // the applied rate can differ from the scheduled one (solver stages, constraints).
    // Called after the container is finalized, so the content is the end-of-step one.
    if (_delay == nullptr) {
        return;
    }

    double delivered = *_outputs[0]->GetAmountPointer();
    double content = _container->GetContentWithoutChanges();
    double inflow = content - _previousContent + delivered;
    _previousContent = content;

    // Spread the inflow over the ordinates, deduct what left from the slot due now, and
    // shift. Any gap between what was due (scheduled plus the same-step share) and what
    // left is carried to the next step, so no water is stranded in the brick.
    double carryOver = _schedule[0] + _ordinates[0] * inflow - delivered;
    for (int j = 0; j < static_cast<int>(_schedule.size()) - 1; ++j) {
        _schedule[j] = _schedule[j + 1] + _ordinates[j + 1] * inflow;
    }
    _schedule.back() = 0.0;
    _schedule[0] += carryOver;

    _processStorage = std::accumulate(_schedule.begin(), _schedule.end(), 0.0);
}

void ProcessRoutingDelay::_recomputeOrdinates() {
    if (_delay == nullptr) {
        return;
    }

    double timeStepInDays = GetTimeStepInDays();
    _lastTimeStep = timeStepInDays;
    _lastDelay = *_delay;

    // The delay on the grid of steps: n whole steps and a fraction f of the next one.
    double steps = std::max(0.0, static_cast<double>(*_delay)) / timeStepInDays;
    auto whole = static_cast<int>(std::floor(steps));
    double fraction = steps - whole;

    // A delay given in whole hours on an hourly step is an exact shift; the float
    // parameter only makes it look fractional, so snap near-integers.
    constexpr double tolerance = 1e-6;
    if (fraction < tolerance) {
        fraction = 0.0;
    } else if (fraction > 1.0 - tolerance) {
        whole += 1;
        fraction = 0.0;
    }

    int size = whole + (fraction > 0.0 ? 2 : 1);
    _ordinates.assign(size, 0.0);
    _ordinates[whole] = 1.0 - fraction;
    if (fraction > 0.0) {
        _ordinates[whole + 1] = fraction;
    }

    // Resize the schedule, keeping what is already in transit: a shorter delay folds the
    // water of the dropped slots into the last one rather than losing it.
    if (static_cast<int>(_schedule.size()) > size) {
        double dropped = std::accumulate(_schedule.begin() + size, _schedule.end(), 0.0);
        _schedule.resize(size);
        _schedule.back() += dropped;
    } else {
        _schedule.resize(size, 0.0);
    }
}
