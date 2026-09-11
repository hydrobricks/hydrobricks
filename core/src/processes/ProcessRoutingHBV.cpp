#include "ProcessRoutingHBV.h"

#include <cmath>
#include <numeric>

#include "Brick.h"
#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessRoutingHBV::ProcessRoutingHBV(WaterContainer* container)
    : ProcessOutflow(container),
      _maxbas(nullptr),
      _lastMaxbas(0.0),
      _lastTimeStep(0.0),
      _previousContent(0.0),
      _processStorage(0.0) {}

void ProcessRoutingHBV::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("maxbas", 1.0f);
    if (modelSettings->LogAll()) {
        modelSettings->AddProcessLogging("internal_storage");
    }
}

bool ProcessRoutingHBV::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (_maxbas == nullptr) {
        LogError("HBV routing process: missing the 'maxbas' parameter.");
        return false;
    }

    return true;
}

void ProcessRoutingHBV::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _maxbas = GetParameterValuePointer(processSettings, "maxbas");
    _recomputeUH();
}

void ProcessRoutingHBV::Reset() {
    ProcessOutflow::Reset();
    _recomputeUH();
    std::fill(_stuh.begin(), _stuh.end(), 0.0);
    _previousContent = 0.0;
    _processStorage = 0.0;
}

double* ProcessRoutingHBV::GetValuePointer(std::string_view name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }
    if (name == "internal_storage") {
        return &_processStorage;
    }

    return nullptr;
}

const vecDouble& ProcessRoutingHBV::GetRates() {
    if (_maxbas == nullptr) {
        return StoreRates({0});
    }

    // Recompute the UH ordinates if maxbas changed (calibration loop) or if the time
    // step is not the one they were built for (the structure is built before the timer
    // is initialized, so the first computation assumes a daily step).
    if (*_maxbas != _lastMaxbas || _currentTimeStepInDays() != _lastTimeStep) {
        _recomputeUH();
    }

    // Scheduled delivery: the amount due this timestep (including any backlog from
    // previous under-deliveries) plus the same-step share of this timestep's inflow.
    double in = _container->SumIncomingFluxes();

    return StoreRates({std::max(0.0, _stuh[0]) + _uhOrd[0] * in});
}

void ProcessRoutingHBV::Finalize() {
    // Advance the delivery schedule exactly once per timestep, based on the water
    // amounts actually committed this timestep. Called after the water container has
    // been finalized, so GetContentWithoutChanges() returns the end-of-step content.
    if (_maxbas == nullptr) {
        return;
    }

    // Water actually delivered to the outlet and actually received this timestep. The
    // applied rate can deviate from the scheduled delivery (solver stage averaging,
    // rate constraints), so both are measured on the committed amounts.
    double delivered = *_outputs[0]->GetAmountPointer();
    double content = _container->GetContentWithoutChanges();
    double inflow = content - _previousContent + delivered;
    _previousContent = content;

    // Distribute the inflow over the triangular ordinates, deduct the delivered
    // amount from the slot due this timestep and shift the schedule. Any difference
    // between the scheduled and the actual delivery is carried over to the next
    // timestep ('due now'), so nothing gets stranded in the routing container.
    double carryOver = _stuh[0] + _uhOrd[0] * inflow - delivered;
    for (int j = 0; j < static_cast<int>(_stuh.size()) - 1; ++j) {
        _stuh[j] = _stuh[j + 1] + _uhOrd[j + 1] * inflow;
    }
    _stuh.back() = 0.0;
    _stuh[0] += carryOver;

    // In-transit water (scheduled, not yet delivered) for water balance logging.
    _processStorage = std::accumulate(_stuh.begin(), _stuh.end(), 0.0);
}

void ProcessRoutingHBV::_recomputeUH() {
    if (_maxbas == nullptr) {
        return;
    }

    // The schedule is a grid of time steps while maxbas is a duration in days, so the
    // base of the triangle is converted to a number of steps: a 3-day maxbas spans 3
    // slots on a daily step and 72 on an hourly one, and the routed shape is the same.
    double timeStepInDays = _currentTimeStepInDays();
    _lastTimeStep = timeStepInDays;
    double maxbasInSteps = static_cast<double>(*_maxbas) / timeStepInDays;
    if (maxbasInSteps < 1.0) {
        maxbasInSteps = 1.0;  // shorter than the time step: pass-through
    }
    _lastMaxbas = *_maxbas;

    int n = static_cast<int>(std::ceil(maxbasInSteps));
    _uhOrd.resize(n);
    for (int j = 1; j <= n; ++j) {
        _uhOrd[j - 1] = _cumulativeWeight(static_cast<double>(j), maxbasInSteps) -
                        _cumulativeWeight(static_cast<double>(j - 1), maxbasInSteps);
    }

    // Resize the delivery schedule, preserving existing state
    _stuh.resize(n, 0.0);
}

double ProcessRoutingHBV::_cumulativeWeight(double t, double maxbas) {
    if (t <= 0.0) return 0.0;
    if (t >= maxbas) return 1.0;
    double ratio = t / maxbas;
    if (ratio <= 0.5) {
        return 2.0 * ratio * ratio;
    }
    return 1.0 - 2.0 * (1.0 - ratio) * (1.0 - ratio);
}

double ProcessRoutingHBV::_currentTimeStepInDays() const {
    if (_timeMachine == nullptr) {
        return 1.0;
    }
    double timeStepInDays = *_timeMachine->GetTimeStepPointer();

    // The timer is initialized after the structure is built, so it can still be unset
    // here; the daily step is then the assumption, and the ordinates are rebuilt on the
    // first call that sees the real one.
    return timeStepInDays > 0 ? timeStepInDays : 1.0;
}
