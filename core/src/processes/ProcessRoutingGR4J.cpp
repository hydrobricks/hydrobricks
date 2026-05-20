#include "ProcessRoutingGR4J.h"

#include <cmath>

#include "Brick.h"
#include "WaterContainer.h"

ProcessRoutingGR4J::ProcessRoutingGR4J(WaterContainer* container)
    : ProcessOutflow(container),
      _exchangeFactor(nullptr),
      _routingCapacity(nullptr),
      _uhBaseTime(nullptr),
      _r(0.0),
      _qr(0.0),
      _qd(0.0) {}

void ProcessRoutingGR4J::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("exchange_factor", 0.0f);
    modelSettings->AddProcessParameter("routing_capacity", 90.0f);
    modelSettings->AddProcessParameter("uh_base_time", 1.7f);
}

void ProcessRoutingGR4J::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _exchangeFactor = GetParameterValuePointer(processSettings, "exchange_factor");
    _routingCapacity = GetParameterValuePointer(processSettings, "routing_capacity");
    _uhBaseTime = GetParameterValuePointer(processSettings, "uh_base_time");
    _recomputeUH();
}

void ProcessRoutingGR4J::Reset() {
    ProcessOutflow::Reset();
    _recomputeUH();
    std::fill(_stuh1.begin(), _stuh1.end(), 0.0);
    std::fill(_stuh2.begin(), _stuh2.end(), 0.0);
    _r = 0.0;
    _qr = 0.0;
    _qd = 0.0;
}

double* ProcessRoutingGR4J::GetValuePointer(std::string_view name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }
    if (name == "q_routing") {
        return &_qr;
    }
    if (name == "q_direct") {
        return &_qd;
    }
    if (name == "routing_store") {
        return &_r;
    }

    return nullptr;
}

vecDouble ProcessRoutingGR4J::GetRates() {
    if (_uhBaseTime == nullptr || _routingCapacity == nullptr || _exchangeFactor == nullptr) {
        return {0};
    }

    double X2 = *_exchangeFactor;
    double X3 = *_routingCapacity;

    // Recompute UH ordinates if X4 changed (calibration loop)
    double currentX4 = *_uhBaseTime;
    if (static_cast<int>(std::ceil(currentX4)) + 1 != static_cast<int>(_uh1Ord.size())) {
        _recomputeUH();
    }

    // PR = this timestep's inflow (Pn-Ps from ground + Perc from production_store).
    // SumIncomingFluxes() reads the flux _amount values set by upstream ApplyChange calls.
    // Ground (direct brick) sets its outflow before the solver runs;
    // production_store (solver brick) updates its outflow in ApplyProcesses.
    double PR = _container->SumIncomingFluxes();

    double prhu1 = 0.9 * PR;
    double prhu2 = 0.1 * PR;

    // Compute UH1 output after a hypothetical advance, WITHOUT modifying _stuh1.
    // After advance: new_stuh1[0] = old_stuh1[1] + uh1Ord[0]*prhu1
    double uh1_out = (_stuh1.size() > 1 ? _stuh1[1] : 0.0) + _uh1Ord[0] * prhu1;

    // Compute UH2 output similarly.
    double uh2_out = (_stuh2.size() > 1 ? _stuh2[1] : 0.0) + _uh2Ord[0] * prhu2;

    // Groundwater exchange using current routing store
    double F = 0.0;
    if (X3 > 0) {
        double rr = _r / X3;
        F = X2 * rr * rr * rr * std::sqrt(rr);  // X2 × (R/X3)^3.5
    }

    // Routing store update (read-only; committed in Finalize)
    double r_pred = std::max(0.0, _r + uh1_out + F);
    double rr4 = 0.0;
    if (X3 > 0) {
        double ratio = r_pred / X3;
        rr4 = ratio * ratio * ratio * ratio;
    }
    _qr = r_pred * (1.0 - std::pow(1.0 + rr4, -0.25));
    _qd = std::max(0.0, uh2_out + F);

    return {_qr + _qd};
}

void ProcessRoutingGR4J::Finalize() {
    // Advance UH buffers and commit routing store update exactly once per timestep.
    // Called after all ApplyProcesses() have run, so SumIncomingFluxes() returns
    // the final, committed inflow for this timestep.
    if (_uhBaseTime == nullptr || _routingCapacity == nullptr || _exchangeFactor == nullptr) {
        return;
    }

    double X2 = *_exchangeFactor;
    double X3 = *_routingCapacity;

    double PR = _container->SumIncomingFluxes();
    double prhu1 = 0.9 * PR;
    double prhu2 = 0.1 * PR;

    // Advance UH1 buffer in-place
    for (int j = 0; j < static_cast<int>(_stuh1.size()) - 1; ++j) {
        _stuh1[j] = _stuh1[j + 1] + _uh1Ord[j] * prhu1;
    }
    _stuh1.back() = _uh1Ord.back() * prhu1;

    // Advance UH2 buffer in-place
    for (int j = 0; j < static_cast<int>(_stuh2.size()) - 1; ++j) {
        _stuh2[j] = _stuh2[j + 1] + _uh2Ord[j] * prhu2;
    }
    _stuh2.back() = _uh2Ord.back() * prhu2;

    // Commit routing store update using the advanced buffer values
    double F = 0.0;
    if (X3 > 0) {
        double rr = _r / X3;
        F = X2 * rr * rr * rr * std::sqrt(rr);
    }
    _r = std::max(0.0, _r + _stuh1[0] + F);
    double rr4 = 0.0;
    if (X3 > 0) {
        double ratio = _r / X3;
        rr4 = ratio * ratio * ratio * ratio;
    }
    _qr = _r * (1.0 - std::pow(1.0 + rr4, -0.25));
    _r -= _qr;
    _qd = std::max(0.0, _stuh2[0] + F);
}

void ProcessRoutingGR4J::_recomputeUH() {
    if (_uhBaseTime == nullptr) {
        return;
    }

    double X4 = *_uhBaseTime;
    if (X4 <= 0) {
        X4 = 0.5;
    }

    int n1 = static_cast<int>(std::ceil(X4));
    int n2 = static_cast<int>(std::ceil(2.0 * X4));

    _uh1Ord.resize(n1);
    for (int j = 1; j <= n1; ++j) {
        _uh1Ord[j - 1] = _sh1(static_cast<double>(j), X4) - _sh1(static_cast<double>(j - 1), X4);
    }

    _uh2Ord.resize(n2);
    for (int j = 1; j <= n2; ++j) {
        _uh2Ord[j - 1] = _sh2(static_cast<double>(j), X4) - _sh2(static_cast<double>(j - 1), X4);
    }

    // Resize buffers, preserving existing state
    _stuh1.resize(n1, 0.0);
    _stuh2.resize(n2, 0.0);
}

double ProcessRoutingGR4J::_sh1(double t, double x4) {
    if (t <= 0.0) return 0.0;
    if (t >= x4) return 1.0;
    return std::pow(t / x4, 2.5);
}

double ProcessRoutingGR4J::_sh2(double t, double x4) {
    if (t <= 0.0) return 0.0;
    if (t >= 2.0 * x4) return 1.0;
    if (t < x4) return 0.5 * std::pow(t / x4, 2.5);
    return 1.0 - 0.5 * std::pow(2.0 - t / x4, 2.5);
}
