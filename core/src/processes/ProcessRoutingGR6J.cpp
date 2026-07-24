#include "ProcessRoutingGR6J.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "Brick.h"
#include "WaterContainer.h"
#include "helpers/GR6JFormulas.h"
#include "helpers/GRUnitHydrograph.h"

ProcessRoutingGR6J::ProcessRoutingGR6J(WaterContainer* container)
    : ProcessOutflow(container),
      _exchangeFactor(nullptr),
      _routingCapacity(nullptr),
      _uhBaseTime(nullptr),
      _exchangeThreshold(nullptr),
      _expStoreCoeff(nullptr),
      _r(0.0),
      _rexp(0.0),
      _qr(0.0),
      _qrexp(0.0),
      _qd(0.0),
      _processStorage(0.0) {
    // The exponential store is bottomless: its level (and hence the uh_input container content that
    // mirrors the routing storage) can be negative. Allow the container to hold negative content so
    // the exponential-store baseflow is emitted in full and the water balance stays consistent.
    _container->SetAllowNegativeContent(true);
}

void ProcessRoutingGR6J::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("exchange_factor", 0.0f);
    modelSettings->AddProcessParameter("routing_capacity", 90.0f);
    modelSettings->AddProcessParameter("uh_base_time", 1.7f);
    modelSettings->AddProcessParameter("exchange_threshold", 0.0f);
    modelSettings->AddProcessParameter("exp_store_coeff", 4.0f);
    if (modelSettings->LogAll()) {
        modelSettings->AddProcessLogging("internal_storage");
    }
}

void ProcessRoutingGR6J::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _exchangeFactor = GetParameterValuePointer(processSettings, "exchange_factor");
    _routingCapacity = GetParameterValuePointer(processSettings, "routing_capacity");
    _uhBaseTime = GetParameterValuePointer(processSettings, "uh_base_time");
    _exchangeThreshold = GetParameterValuePointer(processSettings, "exchange_threshold");
    _expStoreCoeff = GetParameterValuePointer(processSettings, "exp_store_coeff");
    _recomputeUH();
}

void ProcessRoutingGR6J::Reset() {
    ProcessOutflow::Reset();
    _recomputeUH();
    std::fill(_stuh1.begin(), _stuh1.end(), 0.0);
    std::fill(_stuh2.begin(), _stuh2.end(), 0.0);
    _r = 0.0;
    _rexp = 0.0;
    _qr = 0.0;
    _qrexp = 0.0;
    _qd = 0.0;
    _processStorage = 0.0;
}

double* ProcessRoutingGR6J::GetValuePointer(std::string_view name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }
    if (name == "q_routing") {
        return &_qr;
    }
    if (name == "q_exp") {
        return &_qrexp;
    }
    if (name == "q_direct") {
        return &_qd;
    }
    if (name == "routing_store") {
        return &_r;
    }
    if (name == "exp_store") {
        return &_rexp;
    }
    if (name == "internal_storage") {
        return &_processStorage;
    }

    return nullptr;
}

const vecDouble& ProcessRoutingGR6J::GetChangeRates() {
    // Bypass the base-class empty-container short-circuit: the exponential store is bottomless and
    // discharges even when the container is empty (its baseflow at level 0 is X6*ln(2) > 0, and it
    // keeps discharging as it goes negative). The discharge is computed directly from the stores.
    return GetRates();
}

const vecDouble& ProcessRoutingGR6J::GetRates() {
    if (_uhBaseTime == nullptr || _routingCapacity == nullptr || _exchangeFactor == nullptr ||
        _exchangeThreshold == nullptr || _expStoreCoeff == nullptr) {
        return StoreRates({0});
    }

    double X2 = *_exchangeFactor;
    double X3 = *_routingCapacity;
    double X5 = *_exchangeThreshold;
    double X6 = *_expStoreCoeff;

    // Recompute UH ordinates if X4 changed (calibration loop)
    double currentX4 = *_uhBaseTime;
    double clampedX4 = currentX4 <= 0.0 ? 0.5 : currentX4;
    if (static_cast<int>(std::ceil(clampedX4)) != static_cast<int>(_uh1Ord.size())) {
        _recomputeUH();
    }

    // PR = this timestep's inflow (Pn-Ps from ground + Perc from production_store).
    double PR = _container->SumIncomingFluxes();
    double prhu1 = 0.9 * PR;
    double prhu2 = 0.1 * PR;

    // Compute UH outputs after a hypothetical advance, WITHOUT modifying the buffers.
    double uh1_out = (_stuh1.size() > 1 ? _stuh1[1] : 0.0) + (_uh1Ord.empty() ? 0.0 : _uh1Ord[0]) * prhu1;
    double uh2_out = (_stuh2.size() > 1 ? _stuh2[1] : 0.0) + (_uh2Ord.empty() ? 0.0 : _uh2Ord[0]) * prhu2;

    // Threshold groundwater exchange from the current (start-of-step) routing store.
    double F = 0.0;
    if (X3 > 0) {
        F = X2 * (_r / X3 - X5);
    }

    // Power routing store (read-only prediction): receives 60% of UH1 output.
    double r_pred = std::max(0.0, _r + 0.6 * uh1_out + F);
    double rr4 = 0.0;
    if (X3 > 0) {
        double ratio = r_pred / X3;
        rr4 = ratio * ratio * ratio * ratio;
    }
    _qr = r_pred * (1.0 - std::pow(1.0 + rr4, -0.25));

    // Exponential store (read-only prediction): receives 40% of UH1 output; may go negative.
    double rexp_pred = _rexp + 0.4 * uh1_out + F;
    _qrexp = gr6j::ExponentialStoreOutflow(rexp_pred, X6);

    // Direct branch: receives UH2 output.
    _qd = std::max(0.0, uh2_out + F);

    // The discharge is emitted in full even when the container is empty: the exponential store is
    // bottomless and its baseflow (plus carried-over store releases) draws the uh_input container
    // negative, which is allowed (see SetAllowNegativeContent in the constructor). The container
    // content then mirrors the routing storage — including the negative exponential-store level — so
    // both the discharge and the water balance stay consistent with the airGR GR6J formulation.
    return StoreRates({std::max(0.0, _qr + _qrexp + _qd)});
}

void ProcessRoutingGR6J::Finalize() {
    // Advance UH buffers and commit both routing stores exactly once per timestep.
    // Called after all ApplyProcesses() have run, so SumIncomingFluxes() returns the
    // final, committed inflow for this timestep.
    if (_uhBaseTime == nullptr || _routingCapacity == nullptr || _exchangeFactor == nullptr ||
        _exchangeThreshold == nullptr || _expStoreCoeff == nullptr) {
        return;
    }

    double X2 = *_exchangeFactor;
    double X3 = *_routingCapacity;
    double X5 = *_exchangeThreshold;
    double X6 = *_expStoreCoeff;

    double PR = _container->SumIncomingFluxes();
    double prhu1 = 0.9 * PR;
    double prhu2 = 0.1 * PR;

    // Advance UH1 buffer in place
    for (int j = 0; j < static_cast<int>(_stuh1.size()) - 1; ++j) {
        _stuh1[j] = _stuh1[j + 1] + _uh1Ord[j] * prhu1;
    }
    if (!_stuh1.empty()) {
        _stuh1.back() = _uh1Ord.back() * prhu1;
    }

    // Advance UH2 buffer in place
    for (int j = 0; j < static_cast<int>(_stuh2.size()) - 1; ++j) {
        _stuh2[j] = _stuh2[j + 1] + _uh2Ord[j] * prhu2;
    }
    if (!_stuh2.empty()) {
        _stuh2.back() = _uh2Ord.back() * prhu2;
    }

    // Groundwater exchange from the routing store carried over from the previous step.
    double F = 0.0;
    if (X3 > 0) {
        F = X2 * (_r / X3 - X5);
    }

    double uh1_out = _stuh1.empty() ? 0.0 : _stuh1[0];
    double uh2_out = _stuh2.empty() ? 0.0 : _stuh2[0];

    // Power routing store: 60% of UH1 output + exchange.
    _r = std::max(0.0, _r + 0.6 * uh1_out + F);
    double rr4 = 0.0;
    if (X3 > 0) {
        double ratio = _r / X3;
        rr4 = ratio * ratio * ratio * ratio;
    }
    _qr = _r * (1.0 - std::pow(1.0 + rr4, -0.25));
    _r -= _qr;

    // Exponential store: 40% of UH1 output + exchange; may go negative.
    _rexp = _rexp + 0.4 * uh1_out + F;
    _qrexp = gr6j::ExponentialStoreOutflow(_rexp, X6);
    _rexp -= _qrexp;

    // Direct branch.
    _qd = std::max(0.0, uh2_out + F);

    _processStorage = _r + _rexp + std::accumulate(_stuh1.begin(), _stuh1.end(), 0.0) +
                      std::accumulate(_stuh2.begin(), _stuh2.end(), 0.0);
}

void ProcessRoutingGR6J::_recomputeUH() {
    if (_uhBaseTime == nullptr) {
        return;
    }

    gr_uh::ComputeOrdinates(*_uhBaseTime, _uh1Ord, _uh2Ord);

    // Resize buffers to match the ordinate lengths, preserving existing state.
    _stuh1.resize(_uh1Ord.size(), 0.0);
    _stuh2.resize(_uh2Ord.size(), 0.0);
}
