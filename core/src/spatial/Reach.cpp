#include "Reach.h"

#include <cmath>
#include <numeric>

#include "SettingsModel.h"
#include "SubBasin.h"

namespace {
constexpr double kSecondsPerDay = 86400.0;
constexpr int kMaxSubsteps = 100;
constexpr int kMaxSubreaches = 50;
constexpr double kOrdinateTolerance = 1e-6;
// The celerity is kept in a plausible range for a river: a vanishing discharge would otherwise give a
// vanishing celerity and an unbounded travel time.
constexpr double kMinCelerity = 0.05;  // m/s
constexpr double kMaxCelerity = 10.0;  // m/s
// Below this discharge the geometry says nothing about the wave speed (a dry channel).
constexpr double kMinDischarge = 1e-9;  // m3/s
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
    if (name == "muskingum_cunge" || name == "muskingum-cunge" || name == "cunge") {
        return Scheme::MuskingumCunge;
    }
    throw ModelConfigError(std::format(
        "Unknown channel routing scheme '{}' (expected 'none', 'lag', 'muskingum' or 'muskingum_cunge').", name));
}

string Reach::SchemeToString(Scheme scheme) {
    switch (scheme) {
        case Scheme::Lag:
            return "lag";
        case Scheme::Muskingum:
            return "muskingum";
        case Scheme::MuskingumCunge:
            return "muskingum_cunge";
        default:
            return "none";
    }
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
    if (_subbasin->HasProperty("width")) {
        _widthOverride = _subbasin->GetPropertyDouble("width");
    }
    if (_subbasin->HasProperty("manning")) {
        _manningOverride = _subbasin->GetPropertyDouble("manning");
    }
}

void Reach::SetChannelRouting(const ChannelRoutingSettings& settings) {
    _scheme = SchemeFromString(settings.scheme);
    _routeLocal = settings.routeLocalRunoff;
    _celerity = nullptr;
    _celerityExponent = nullptr;
    _referenceDischarge = nullptr;
    _x = nullptr;
    _width = nullptr;
    _manning = nullptr;
    for (const auto& parameter : settings.parameters) {
        const string& name = parameter.GetName();
        if (name == "celerity") {
            _celerity = parameter.GetValuePointer();
        } else if (name == "celerity_exponent") {
            _celerityExponent = parameter.GetValuePointer();
        } else if (name == "reference_discharge") {
            _referenceDischarge = parameter.GetValuePointer();
        } else if (name == "x") {
            _x = parameter.GetValuePointer();
        } else if (name == "width") {
            _width = parameter.GetValuePointer();
        } else if (name == "manning") {
            _manning = parameter.GetValuePointer();
        }
    }
    if (_scheme != Scheme::None && _celerity == nullptr && std::isnan(_celerityOverride)) {
        throw ModelConfigError("The channel routing scheme needs the 'celerity' parameter.");
    }
    if (_scheme == Scheme::Muskingum && _x == nullptr && std::isnan(_xOverride)) {
        throw ModelConfigError("The Muskingum channel routing scheme needs the 'x' parameter.");
    }
    if (_scheme == Scheme::MuskingumCunge && _length > 0 && _slope <= 0) {
        throw ModelConfigError(
            std::format("The Muskingum-Cunge channel routing needs the slope of the reach of subbasin {}. The "
                        "subbasin delineation provides it ('slope', in m/m); add it to the subbasin table.",
                        _subbasin->GetId()));
    }
    if (_scheme != Scheme::None && _length <= 0) {
        LogWarning("Subbasin {} has no reach length: its routing is instantaneous.", _subbasin->GetId());
    }
    // Force the recomputation of the ordinates / coefficients on the next step, and of the sub reach count:
    // the parameters it is derived from (the reference discharge, the width) may have just changed, as they
    // do between the runs of a calibration.
    _main.lastTravelTime = -1;
    _main.lastX = -1;
    _main.lastTimeStep = -1;
    _main.subreachTimeStep = -1;
    _local.lastTravelTime = -1;
    _local.lastX = -1;
    _local.lastTimeStep = -1;
    _local.subreachTimeStep = -1;
}

double Reach::GetWidth() const {
    if (!std::isnan(_widthOverride)) {
        return _widthOverride;
    }
    return _width ? static_cast<double>(*_width) : 10.0;
}

double Reach::GetManning() const {
    if (!std::isnan(_manningOverride)) {
        return _manningOverride;
    }
    return _manning ? static_cast<double>(*_manning) : 0.035;
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

double Reach::GetCelerityForDischarge(double dischargeM3s) const {
    if (_scheme == Scheme::MuskingumCunge) {
        double width = GetWidth();
        double manning = GetManning();
        if (dischargeM3s <= kMinDischarge || width <= 0 || manning <= 0 || _slope <= 0) {
            return GetCelerity();
        }
        // Manning for a wide rectangular channel: the depth from the discharge, then the kinematic wave
        // celerity, 5/3 of the mean velocity (dQ/dA of the Manning relation).
        double depth = std::pow(dischargeM3s * manning / (width * std::sqrt(_slope)), 0.6);
        double velocity = dischargeM3s / (width * depth);
        return std::clamp(5.0 * velocity / 3.0, kMinCelerity, kMaxCelerity);
    }

    double celerity = GetCelerity();
    double exponent = _celerityExponent ? static_cast<double>(*_celerityExponent) : 0.0;
    if (exponent == 0.0 || dischargeM3s <= kMinDischarge) {
        return celerity;
    }
    double reference = _referenceDischarge ? static_cast<double>(*_referenceDischarge) : 1.0;
    if (reference <= 0) {
        return celerity;
    }

    return std::clamp(celerity * std::pow(dischargeM3s / reference, exponent), kMinCelerity, kMaxCelerity);
}

double Reach::GetTravelTimeInDays() const {
    // The celerity AT the reference discharge, not the reference celerity: the two are the same for the
    // schemes taking the celerity as a parameter, but Muskingum-Cunge derives it from the geometry and
    // ignores the parameter entirely.
    double discharge = _referenceDischarge ? static_cast<double>(*_referenceDischarge) : 1.0;
    double celerity = GetCelerityForDischarge(discharge);
    if (_length <= 0 || celerity <= 0) {
        return 0.0;
    }
    return _length / (celerity * kSecondsPerDay);
}

void Reach::ComputeCungeParameters(double dischargeM3s, double length, double& travelTime, double& x) const {
    double celerity = GetCelerityForDischarge(dischargeM3s);
    travelTime = length / (celerity * kSecondsPerDay);

    // Cunge's weighting factor: the numerical diffusion of the scheme matches the physical diffusion of the
    // diffusive wave, X = 0.5 (1 - Q / (B S0 c L)). A dry channel gives no information: keep the parameter.
    double width = GetWidth();
    if (dischargeM3s <= kMinDischarge || width <= 0 || _slope <= 0 || length <= 0) {
        x = GetMuskingumX();
        return;
    }
    double unitDischarge = dischargeM3s / width;
    x = 0.5 * (1.0 - unitDischarge / (_slope * celerity * length));
    x = std::clamp(x, 0.0, 0.5);
}

int Reach::ComputeSubreachCount(double length, double timeStepInDays) const {
    if (_scheme != Scheme::MuskingumCunge || length <= 0 || _slope <= 0 || timeStepInDays <= 0) {
        return 1;
    }
    double discharge = _referenceDischarge ? static_cast<double>(*_referenceDischarge) : 1.0;
    double width = GetWidth();
    if (discharge <= kMinDischarge || width <= 0) {
        return 1;
    }

    // Ponce and Theurer (1982): the sub reach may not be longer than half the sum of the distance the wave
    // travels in one time step and the length at which the weighting factor X vanishes. Beyond it the grid is
    // too coarse for the wave and the scheme diffuses it numerically.
    double celerity = GetCelerityForDischarge(discharge);
    double unitDischarge = discharge / width;
    double maxLength = 0.5 * (celerity * timeStepInDays * kSecondsPerDay + unitDischarge / (_slope * celerity));
    if (maxLength <= 0 || length <= maxLength) {
        return 1;
    }

    int count = static_cast<int>(std::ceil(length / maxLength));
    if (count > kMaxSubreaches) {
        LogWarning(
            "Subbasin {}: the Muskingum-Cunge criterion asks for {} sub reaches of the {:.0f} m reach; "
            "keeping {}. Consider splitting the subbasin.",
            _subbasin->GetId(), count, length, kMaxSubreaches);
        return kMaxSubreaches;
    }

    return count;
}

void Reach::SetSubreachCount(RoutingState& state, int count) {
    count = std::max(1, count);
    if (state.subreaches == count && static_cast<int>(state.subStorage.size()) == count) {
        return;
    }

    double total = state.subStorage.empty() ? state.storage
                                            : std::accumulate(state.subStorage.begin(), state.subStorage.end(), 0.0);
    state.subreaches = count;
    state.subStorage.assign(count, total / count);
    state.previousInflow.assign(count, 0.0);
    state.previousOutflow.assign(count, 0.0);
    state.storage = total;
}

double Reach::RouteUpstream(double inflowVolume, double timeStepInDays) {
    _inflow = inflowVolume;
    _outflow = RouteBranch(_main, inflowVolume, _length, timeStepInDays);

    return _outflow;
}

double Reach::RouteLocal(double localVolume, double timeStepInDays) {
    if (!RoutesLocalRunoff()) {
        return localVolume;
    }

    // Generated uniformly along the reach, the local runoff travels half of it on average.
    double routed = RouteBranch(_local, localVolume, 0.5 * _length, timeStepInDays);
    _inflow += localVolume;
    _outflow += routed;

    return routed;
}

double Reach::RouteBranch(RoutingState& state, double volume, double length, double timeStepInDays) {
    if (_scheme == Scheme::None || length <= 0) {
        state.storage = 0;
        return volume;
    }

    // The discharge of the step drives the celerity of the discharge-dependent schemes.
    double dischargeM3s = volume / (timeStepInDays * kSecondsPerDay);

    if (_scheme == Scheme::MuskingumCunge) {
        // The wave is resolved on sub reaches short enough for it; K and X are those of one sub reach. The
        // count is fixed for the run: it depends on the reference discharge and the time step, not on the
        // discharge of the step, because every sub reach carries state.
        if (timeStepInDays != state.subreachTimeStep) {
            SetSubreachCount(state, ComputeSubreachCount(length, timeStepInDays));
            state.subreachTimeStep = timeStepInDays;
        }
        double travelTime = 0;
        double x = 0;
        ComputeCungeParameters(dischargeM3s, length / state.subreaches, travelTime, x);
        return RouteMuskingum(state, volume, travelTime, x, timeStepInDays);
    }

    double celerity = GetCelerityForDischarge(dischargeM3s);
    double travelTime = celerity > 0 ? length / (celerity * kSecondsPerDay) : 0.0;
    if (_scheme == Scheme::Lag) {
        return RouteLag(state, volume, travelTime, timeStepInDays);
    }

    return RouteMuskingum(state, volume, travelTime, GetMuskingumX(), timeStepInDays);
}

void Reach::ComputeOrdinates(RoutingState& state, double travelTime, double timeStepInDays) {
    state.lastTravelTime = travelTime;
    state.lastTimeStep = timeStepInDays;

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
    state.ordinates.assign(size, 0.0);
    state.ordinates[whole] = 1.0 - fraction;
    if (fraction > 0.0) {
        state.ordinates[whole + 1] = fraction;
    }

    // Resize the schedule, keeping what is already in transit: a shorter travel time folds the water of the
    // dropped slots into the last one rather than losing it.
    if (static_cast<int>(state.schedule.size()) > size) {
        double dropped = std::accumulate(state.schedule.begin() + size, state.schedule.end(), 0.0);
        state.schedule.resize(size);
        state.schedule.back() += dropped;
    } else {
        state.schedule.resize(size, 0.0);
    }
}

double Reach::RouteLag(RoutingState& state, double volume, double travelTime, double timeStepInDays) {
    if (travelTime != state.lastTravelTime || timeStepInDays != state.lastTimeStep) {
        ComputeOrdinates(state, travelTime, timeStepInDays);
    }

    for (size_t j = 0; j < state.ordinates.size(); ++j) {
        state.schedule[j] += state.ordinates[j] * volume;
    }
    double outflow = state.schedule[0];
    for (size_t j = 0; j + 1 < state.schedule.size(); ++j) {
        state.schedule[j] = state.schedule[j + 1];
    }
    state.schedule.back() = 0.0;
    state.storage = std::accumulate(state.schedule.begin(), state.schedule.end(), 0.0);

    return outflow;
}

void Reach::ComputeCoefficients(RoutingState& state, double travelTime, double x, double timeStepInDays) const {
    state.lastTravelTime = travelTime;
    state.lastX = x;
    state.lastTimeStep = timeStepInDays;
    state.instantaneous = false;

    if (travelTime <= 0) {
        state.instantaneous = true;
        return;
    }
    if (x < 0 || x > 0.5) {
        throw ModelConfigError(std::format("The Muskingum weighting factor must be in [0, 0.5] (got {}).", x));
    }

    // The scheme is stable and free of negative outflows when 2KX <= dt <= 2K(1-X). The upper bound is met by
    // sub-stepping (n sub-steps of dt/n); a reach far shorter than one step is treated as instantaneous.
    double upper = 2.0 * travelTime * (1.0 - x);
    double substeps = std::ceil(timeStepInDays / upper);
    if (substeps > kMaxSubsteps) {
        LogDebug(
            "Subbasin {}: the reach travel time ({:.3g} d) is far shorter than the time step; its routing "
            "is instantaneous.",
            _subbasin->GetId(), travelTime);
        state.instantaneous = true;
        return;
    }
    int newSubsteps = std::max(1, static_cast<int>(substeps));
    double dt = timeStepInDays / newSubsteps;
    double denominator = 2.0 * travelTime * (1.0 - x) + dt;
    state.c0 = (dt - 2.0 * travelTime * x) / denominator;
    state.c1 = (dt + 2.0 * travelTime * x) / denominator;
    state.c2 = (2.0 * travelTime * (1.0 - x) - dt) / denominator;

    if (state.c0 < 0) {
        // dt < 2KX: the response to a rising inflow starts negative and is clamped at zero, which delays the
        // first outflow slightly. Mass is conserved (the storage keeps the difference).
        LogDebug(
            "Subbasin {}: the time step ({:.3g} d) is shorter than 2KX ({:.3g} d); the first Muskingum "
            "response is clamped at zero.",
            _subbasin->GetId(), dt, 2.0 * travelTime * x);
    }

    // The previous values are per sub-step: they only carry over while the sub-step length is unchanged. With
    // the discharge-dependent schemes the coefficients are recomputed at every step, so they must not be
    // dropped each time, or the scheme would lose its memory of the routed wave.
    if (newSubsteps != state.substeps) {
        state.previousInflow.assign(state.previousInflow.size(), 0.0);
        state.previousOutflow.assign(state.previousOutflow.size(), 0.0);
    }
    state.substeps = newSubsteps;
}

double Reach::RouteMuskingum(RoutingState& state, double volume, double travelTime, double x, double timeStepInDays) {
    // The state vectors must exist and match the sub reach count before anything reads them.
    if (static_cast<int>(state.subStorage.size()) != state.subreaches) {
        SetSubreachCount(state, state.subreaches);
    }
    if (travelTime != state.lastTravelTime || x != state.lastX || timeStepInDays != state.lastTimeStep) {
        ComputeCoefficients(state, travelTime, x, timeStepInDays);
    }
    if (state.instantaneous) {
        state.storage = 0;
        std::fill(state.subStorage.begin(), state.subStorage.end(), 0.0);
        return volume;
    }

    double inflow = volume / state.substeps;
    double outflow = 0;
    for (int i = 0; i < state.substeps; ++i) {
        // The sub reaches in series: what leaves one enters the next within the same sub-step.
        double passing = inflow;
        for (int r = 0; r < state.subreaches; ++r) {
            double out = state.c0 * passing + state.c1 * state.previousInflow[r] + state.c2 * state.previousOutflow[r];
            // Never negative, never more than what the sub reach holds: the storage stays the exact balance.
            out = std::max(0.0, std::min(out, state.subStorage[r] + passing));
            state.subStorage[r] += passing - out;
            state.previousInflow[r] = passing;
            state.previousOutflow[r] = out;
            passing = out;
        }
        outflow += passing;
    }
    state.storage = std::accumulate(state.subStorage.begin(), state.subStorage.end(), 0.0);

    return outflow;
}

void Reach::ResetState(RoutingState& state) {
    state.schedule = state.initialSchedule;
    // The schedule must stay aligned with the ordinates: no state may have been saved (empty initial
    // schedule), or the travel time may have changed since. Fold any excess into the last slot, pad with
    // zeros otherwise.
    if (!state.ordinates.empty() && state.schedule.size() > state.ordinates.size()) {
        double dropped = std::accumulate(state.schedule.begin() + static_cast<long>(state.ordinates.size()),
                                         state.schedule.end(), 0.0);
        state.schedule.resize(state.ordinates.size());
        state.schedule.back() += dropped;
    }
    state.schedule.resize(state.ordinates.size(), 0.0);

    // The sub reach state must stay sized like the sub reach count, whether or not a state was saved on this
    // discretization: an undersized vector would be written past its end on the next routing.
    int count = std::max(1, state.subreaches);
    if (static_cast<int>(state.initialSubStorage.size()) == count) {
        state.subStorage = state.initialSubStorage;
    } else {
        state.subStorage.assign(count, state.initialStorage / count);
    }
    state.previousInflow.assign(count, 0.0);
    state.previousOutflow.assign(count, 0.0);
}

void Reach::Reset() {
    _inflow = 0;
    _outflow = 0;
    ResetState(_main);
    ResetState(_local);
    bool isLag = _scheme == Scheme::Lag;
    _main.storage = isLag ? std::accumulate(_main.schedule.begin(), _main.schedule.end(), 0.0) : _main.initialStorage;
    _local.storage = isLag ? std::accumulate(_local.schedule.begin(), _local.schedule.end(), 0.0)
                           : _local.initialStorage;
}

void Reach::SaveAsInitialState() {
    _main.initialStorage = _main.storage;
    _main.initialSchedule = _main.schedule;
    _main.initialSubStorage = _main.subStorage;
    _local.initialStorage = _local.storage;
    _local.initialSchedule = _local.schedule;
    _local.initialSubStorage = _local.subStorage;
}
