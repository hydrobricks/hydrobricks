#include "ProcessRoutingHBV.h"

#include <cmath>
#include <numeric>

#include "Brick.h"
#include "WaterContainer.h"

ProcessRoutingHBV::ProcessRoutingHBV(WaterContainer* container)
    : ProcessOutflow(container),
      _maxbas(nullptr),
      _lastMaxbas(0.0),
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

vecDouble ProcessRoutingHBV::GetRates() {
    if (_maxbas == nullptr) {
        return {0};
    }

    // Recompute UH ordinates if maxbas changed (calibration loop)
    if (*_maxbas != _lastMaxbas) {
        _recomputeUH();
    }

    // Scheduled delivery: the amount due this timestep (including any backlog from
    // previous under-deliveries) plus the same-step share of this timestep's inflow.
    double in = _container->SumIncomingFluxes();

    return {std::max(0.0, _stuh[0]) + _uhOrd[0] * in};
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

    double maxbas = *_maxbas;
    if (maxbas < 1.0) {
        maxbas = 1.0;  // shorter than the timestep: pass-through
    }
    _lastMaxbas = *_maxbas;

    int n = static_cast<int>(std::ceil(maxbas));
    _uhOrd.resize(n);
    for (int j = 1; j <= n; ++j) {
        _uhOrd[j - 1] = _cumulativeWeight(static_cast<double>(j), maxbas) -
                        _cumulativeWeight(static_cast<double>(j - 1), maxbas);
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
