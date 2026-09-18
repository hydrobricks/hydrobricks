#include "Reach.h"

#include <cmath>
#include <numeric>

#include "SettingsModel.h"
#include "SubBasin.h"

namespace {
constexpr double kSecondsPerDay = 86400.0;
constexpr int kMaxSubsteps = 100;
constexpr double kOrdinateTolerance = 1e-6;
}  // namespace

Reach::Reach(SubBasin* subbasin)
    : _subbasin(subbasin) {
    assert(subbasin);
}

Reach::Scheme Reach::SchemeFromString(const string& name) {
    if (name.empty() || name == "none" || name == "instantaneous") {
        return Scheme::None;
    }
    if (name == "lag" || name == "translation" || name == "delay") {
        return Scheme::Lag;
    }
    if (name == "muskingum") {
        return Scheme::Muskingum;
    }
    throw ModelConfigError(std::format("Unknown routing scheme '{}' (expected 'none', 'lag' or 'muskingum').", name));
}

void Reach::Initialize() {
    if (_subbasin->HasProperty("length")) {
        _length = _subbasin->GetPropertyDouble("length");
    }
    if (_subbasin->HasProperty("slope")) {
        _slope = _subbasin->GetPropertyDouble("slope");
    }
    if (_subbasin->HasProperty("celerity")) {
        _celerityOverride = _subbasin->GetPropertyDouble("celerity");
    }
    if (_subbasin->HasProperty("muskingum_x")) {
        _xOverride = _subbasin->GetPropertyDouble("muskingum_x");
    }
}

void Reach::SetRouting(const RoutingSettings& settings) {
    _scheme = SchemeFromString(settings.scheme);
    _celerity = nullptr;
    _x = nullptr;
    for (const auto& parameter : settings.parameters) {
        if (parameter.GetName() == "celerity") {
            _celerity = parameter.GetValuePointer();
        } else if (parameter.GetName() == "x") {
            _x = parameter.GetValuePointer();
        }
    }
    if (_scheme != Scheme::None && _celerity == nullptr && std::isnan(_celerityOverride)) {
        throw ModelConfigError("The routing scheme needs the 'celerity' parameter.");
    }
    if (_scheme == Scheme::Muskingum && _x == nullptr && std::isnan(_xOverride)) {
        throw ModelConfigError("The Muskingum routing scheme needs the 'x' parameter.");
    }
    if (_scheme != Scheme::None && _length <= 0) {
        LogWarning("Subbasin {} has no reach length: its routing is instantaneous.", _subbasin->GetId());
    }
    // Force the recomputation of the ordinates / coefficients on the next step.
    _lastTravelTime = -1;
    _lastX = -1;
    _lastTimeStep = -1;
}

double Reach::GetCelerity() const {
    if (!std::isnan(_celerityOverride)) {
        return _celerityOverride;
    }
    return _celerity ? static_cast<double>(*_celerity) : 1.0;
}

double Reach::GetMuskingumX() const {
    if (!std::isnan(_xOverride)) {
        return _xOverride;
    }
    return _x ? static_cast<double>(*_x) : 0.2;
}

double Reach::GetTravelTimeInDays() const {
    double celerity = GetCelerity();
    if (_length <= 0 || celerity <= 0) {
        return 0.0;
    }
    return _length / (celerity * kSecondsPerDay);
}

double Reach::Route(double inflowVolume, double timeStepInDays) {
    _inflow = inflowVolume;

    switch (_scheme) {
        case Scheme::None:
            // Instantaneous: what comes in leaves in the same step, nothing stays in transit.
            _outflow = _inflow;
            _storage = 0;
            break;
        case Scheme::Lag:
            _outflow = RouteLag(inflowVolume, timeStepInDays);
            break;
        case Scheme::Muskingum:
            _outflow = RouteMuskingum(inflowVolume, timeStepInDays);
            break;
    }

    return _outflow;
}

void Reach::ComputeOrdinates(double timeStepInDays) {
    double travelTime = GetTravelTimeInDays();
    _lastTravelTime = travelTime;
    _lastTimeStep = timeStepInDays;

    // The travel time on the grid of steps: n whole steps and a fraction f of the next one. The water
    // received during a step is a block one step long; shifting it by n + f steps puts a share 1 - f of it in
    // slot n and f in slot n + 1 (same logic as the translation delay process).
    double steps = travelTime / timeStepInDays;
    auto whole = static_cast<int>(std::floor(steps));
    double fraction = steps - whole;
    if (fraction < kOrdinateTolerance) {
        fraction = 0.0;
    } else if (fraction > 1.0 - kOrdinateTolerance) {
        whole += 1;
        fraction = 0.0;
    }

    int size = whole + (fraction > 0.0 ? 2 : 1);
    _ordinates.assign(size, 0.0);
    _ordinates[whole] = 1.0 - fraction;
    if (fraction > 0.0) {
        _ordinates[whole + 1] = fraction;
    }

    // Resize the schedule, keeping what is already in transit: a shorter travel time folds the water of the
    // dropped slots into the last one rather than losing it.
    if (static_cast<int>(_schedule.size()) > size) {
        double dropped = std::accumulate(_schedule.begin() + size, _schedule.end(), 0.0);
        _schedule.resize(size);
        _schedule.back() += dropped;
    } else {
        _schedule.resize(size, 0.0);
    }
}

double Reach::RouteLag(double inflowVolume, double timeStepInDays) {
    if (GetTravelTimeInDays() != _lastTravelTime || timeStepInDays != _lastTimeStep) {
        ComputeOrdinates(timeStepInDays);
    }

    for (size_t j = 0; j < _ordinates.size(); ++j) {
        _schedule[j] += _ordinates[j] * inflowVolume;
    }
    double outflow = _schedule[0];
    for (size_t j = 0; j + 1 < _schedule.size(); ++j) {
        _schedule[j] = _schedule[j + 1];
    }
    _schedule.back() = 0.0;
    _storage = std::accumulate(_schedule.begin(), _schedule.end(), 0.0);

    return outflow;
}

void Reach::ComputeCoefficients(double timeStepInDays) {
    double k = GetTravelTimeInDays();
    double x = GetMuskingumX();
    _lastTravelTime = k;
    _lastX = x;
    _lastTimeStep = timeStepInDays;
    _instantaneous = false;

    if (k <= 0) {
        _instantaneous = true;
        return;
    }
    if (x < 0 || x > 0.5) {
        throw ModelConfigError(std::format("The Muskingum weighting factor must be in [0, 0.5] (got {}).", x));
    }

    // The scheme is stable and free of negative outflows when 2KX <= dt <= 2K(1-X). The upper bound is met by
    // sub-stepping (n sub-steps of dt/n); a reach far shorter than one step is treated as instantaneous.
    double upper = 2.0 * k * (1.0 - x);
    double substeps = std::ceil(timeStepInDays / upper);
    if (substeps > kMaxSubsteps) {
        LogDebug(
            "Subbasin {}: the reach travel time ({:.3g} d) is far shorter than the time step; its routing "
            "is instantaneous.",
            _subbasin->GetId(), k);
        _instantaneous = true;
        return;
    }
    _substeps = std::max(1, static_cast<int>(substeps));
    double dt = timeStepInDays / _substeps;
    double denominator = 2.0 * k * (1.0 - x) + dt;
    _c0 = (dt - 2.0 * k * x) / denominator;
    _c1 = (dt + 2.0 * k * x) / denominator;
    _c2 = (2.0 * k * (1.0 - x) - dt) / denominator;

    if (_c0 < 0) {
        // dt < 2KX: the response to a rising inflow starts negative and is clamped at zero, which delays the
        // first outflow slightly. Mass is conserved (the storage keeps the difference).
        LogDebug(
            "Subbasin {}: the time step ({:.3g} d) is shorter than 2KX ({:.3g} d); the first Muskingum "
            "response is clamped at zero.",
            _subbasin->GetId(), dt, 2.0 * k * x);
    }

    // The previous sub-step values refer to the former sub-step length; start afresh.
    _previousInflow = 0;
    _previousOutflow = 0;
}

double Reach::RouteMuskingum(double inflowVolume, double timeStepInDays) {
    if (GetTravelTimeInDays() != _lastTravelTime || GetMuskingumX() != _lastX || timeStepInDays != _lastTimeStep) {
        ComputeCoefficients(timeStepInDays);
    }
    if (_instantaneous) {
        _storage = 0;
        return inflowVolume;
    }

    double inflow = inflowVolume / _substeps;
    double outflow = 0;
    for (int i = 0; i < _substeps; ++i) {
        double out = _c0 * inflow + _c1 * _previousInflow + _c2 * _previousOutflow;
        // Never negative, never more than what the reach holds: the storage stays the exact balance.
        out = std::max(0.0, std::min(out, _storage + inflow));
        _storage += inflow - out;
        _previousInflow = inflow;
        _previousOutflow = out;
        outflow += out;
    }

    return outflow;
}

void Reach::Reset() {
    _inflow = 0;
    _outflow = 0;
    _storage = _initialStorage;
    _schedule = _initialSchedule;
    _previousInflow = 0;
    _previousOutflow = 0;
}

void Reach::SaveAsInitialState() {
    _initialStorage = _storage;
    _initialSchedule = _schedule;
}
