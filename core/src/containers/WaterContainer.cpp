#include "WaterContainer.h"

#include "Brick.h"
#include "FluxToBrickInstantaneous.h"

WaterContainer::WaterContainer(Brick* brick)
    : _content(0),
      _contentChangeDynamic(0),
      _contentChangeStatic(0),
      _initialState(0),
      _capacity(nullptr),
      _infiniteStorage(false),
      _allowNegativeContent(false),
      _inputsBooked(false),
      _constraintCacheBuilt(false),
      _parent(brick),
      _overflow(nullptr) {}

bool WaterContainer::IsValid(bool checkProcesses) const {
    if (!checkProcesses) {
        return true;
    }

    for (int i = 0; i < GetParentBrick()->GetProcessCount(); ++i) {
        auto process = GetParentBrick()->GetProcess(i);
        if (process->GetWaterContainer() == this) {
            return true;
        }
    }
    LogError("A container of the brick {} has no process attached.", GetParentBrick()->GetName());

    return false;
}

void WaterContainer::Validate() const {
    if (!IsValid()) {
        throw ModelConfigError(
            std::format("A container of the brick {} has no process attached.", GetParentBrick()->GetName()));
    }
}

void WaterContainer::SubtractAmountFromDynamicContentChange(double change) {
    if (_infiniteStorage) return;
    _contentChangeDynamic -= change;
}

void WaterContainer::AddAmountToDynamicContentChange(double change) {
    if (_infiniteStorage) return;
    _contentChangeDynamic += change;
}

void WaterContainer::AddAmountToStaticContentChange(double change) {
    if (_infiniteStorage) return;
    _contentChangeStatic += change;
}

void WaterContainer::BuildConstraintCache() {
    _constraintCacheBuilt = true;

    for (int i = 0; i < _parent->GetProcessCount(); ++i) {
        auto process = _parent->GetProcess(i);
        if (process->GetWaterContainer() != this) {
            continue;
        }
        const bool priority = process->HasConstraintPriority();
        for (int j = 0; j < process->GetOutputFluxCount(); ++j) {
            _outgoingFluxes.push_back({process->GetOutputFlux(j), process, priority});
        }
    }

    for (auto& input : _inputs) {
        if (input->IsInstantaneous()) {
            _incomingInstantFluxes.push_back(dynamic_cast<FluxToBrickInstantaneous*>(input));
            continue;
        }
        if (input->IsForcing() || input->IsStatic()) {
            _incomingAmountFluxes.push_back(input);
            continue;
        }
        _incomingRateFluxes.push_back(input);
    }
}

void WaterContainer::ApplyConstraints(double timeStep) {
    if (_infiniteStorage) return;

    if (!_constraintCacheBuilt) {
        BuildConstraintCache();
    }

    // Change rates are quantities per unit time: the content update integrates them over the
    // timestep as content + rate * timeStep (see below). Processes that need to move an absolute
    // amount in one step must therefore divide that amount by the timestep when reporting their
    // rate (e.g. ProcessOutflowSnowHolding). Here we clamp those rates so the content stays within
    // bounds (no negative content, and below the maximum capacity) over the timestep.

    // Get outgoing change rates
    double outputs = 0;
    double priorityOutputs = 0;
    for (const auto& link : _outgoingFluxes) {
        double* changeRate = link.flux->GetChangeRatePointer();
        if (changeRate == nullptr) {
            // For example when the originating brick has an area = 0.
            continue;
        }
        assert(*changeRate < 10000);
        if (*changeRate < 0) {
            *changeRate = 0;
        } else if (*changeRate > 10000) {
            throw RuntimeError(
                std::format("Change rate {} in process {} is too high.", *changeRate, link.process->GetName()));
        }
        assert(GreaterThanOrEqual(*changeRate, 0, EPSILON_D));
        outputs += *changeRate;
        if (link.priority) {
            priorityOutputs += *changeRate;
        }
    }

    // Get incoming change rates. The amount-carrying inputs (forcing, static and
    // instantaneous fluxes) are added separately as inputsStatic, but only when the parent
    // brick has not booked them into the content changes yet (BookIncomingFluxes). In the
    // direct pass the brick books its inputs before the constraints are enforced, so the
    // content read below already carries them and counting the amounts again would let the
    // outgoing rates draw on water that is not there (negative content at Finalize).
    double inputs = 0;
    double inputsStatic = 0;
    if (!_inputsBooked) {
        for (auto flux : _incomingInstantFluxes) {
            inputsStatic += flux->GetRealAmount();
        }
        for (auto flux : _incomingAmountFluxes) {
            inputsStatic += flux->GetAmount();
        }
    }
    for (auto flux : _incomingRateFluxes) {
        double* changeRate = flux->GetChangeRatePointer();
        if (changeRate == nullptr) {
            // For example when the originating brick has an area = 0.
            continue;
        }
        assert(*changeRate < 1000);
        if (*changeRate < 0) {
            *changeRate = 0;
        }
        assert(GreaterThanOrEqual(*changeRate, 0, EPSILON_D));
        inputs += *changeRate;
    }

    double change = inputs - outputs;
    // Once the inputs are booked, the static changes are part of the content; otherwise they
    // are still to come and are accounted for through inputsStatic.
    double content = _inputsBooked ? GetContentWithChanges() : GetContentWithDynamicChanges();

    // Avoid negative content (unless the container is allowed to go negative, e.g. a bottomless
    // routing store whose level can be negative).
    if (!_allowNegativeContent && change < 0 && content + inputsStatic + change * timeStep < 0) {
        // Maximum total outgoing rate the available water can support over the timestep.
        double availRate = std::max(0.0, (content + inputsStatic) / timeStep + inputs);
        // Priority processes (Process::HasConstraintPriority) are served first, up to the
        // available water; the other rates share the remainder proportionally. Without
        // priority processes this reduces to the plain proportional scaling.
        double priorityFactor = 1.0;
        double normalFactor = 0.0;
        double normalOutputs = outputs - priorityOutputs;
        if (priorityOutputs >= availRate) {
            priorityFactor = priorityOutputs > 0 ? availRate / priorityOutputs : 0.0;
        } else if (normalOutputs > 0) {
            normalFactor = (availRate - priorityOutputs) / normalOutputs;
        }
        for (const auto& link : _outgoingFluxes) {
            double* rate = link.flux->GetChangeRatePointer();
            if (rate == nullptr) {
                continue;
            }
            assert(*rate < 1000);
            assert(GreaterThanOrEqual(*rate, 0, EPSILON_D));
            if (NearlyZero(*rate, EPSILON_D)) {
                continue;
            }
            *rate *= link.priority ? priorityFactor : normalFactor;
        }
    }

    // Enforce maximum capacity
    if (HasMaximumCapacity()) {
        if (content + inputsStatic + change * timeStep > *_capacity) {
            double diff = (content + inputsStatic + change * timeStep - *_capacity) / timeStep;
            // If it has an overflow, use it
            if (HasOverflow()) {
                if (_overflow->GetOutputFlux(0)->GetChangeRatePointer() != nullptr) {
                    *(_overflow->GetOutputFlux(0)->GetChangeRatePointer()) = diff;
                    return;
                }
                throw ShouldNotHappen(
                    "WaterContainer::ApplyConstraints - Overflow exists but has no change rate pointer");
            }
            // Check that it is not only due to forcing
            if (content + inputsStatic > *_capacity) {
                throw ModelConfigError(
                    "Forcing is coming directly into a brick with limited capacity and no overflow.");
            }
            // Limit the different rates proportionally
            for (auto flux : _incomingRateFluxes) {
                double* rate = flux->GetChangeRatePointer();
                if (rate == nullptr) {
                    continue;
                }
                assert(*rate < 1000);
                assert(GreaterThanOrEqual(*rate, 0, EPSILON_D));
                if (NearlyZero(*rate, EPSILON_D)) {
                    continue;
                }
                *rate -= diff * std::abs((*rate) / inputs);
            }
        }
    }
}

void WaterContainer::SetOutgoingRatesToZero() {
    for (int i = 0; i < _parent->GetProcessCount(); ++i) {
        auto process = _parent->GetProcess(i);
        if (process->GetWaterContainer() != this) {
            continue;
        }
        for (int j = 0; j < process->GetOutputFluxCount(); ++j) {
            Flux* flux = process->GetOutputFlux(j);
            double* changeRate = flux->GetChangeRatePointer();
            if (changeRate == nullptr) {
                // For example when the originating brick has an area = 0.
                continue;
            }
            assert(changeRate);
            *changeRate = 0;
        }
    }
}

void WaterContainer::Finalize() {
    _inputsBooked = false;
    if (_infiniteStorage) return;
    _content += _contentChangeDynamic + _contentChangeStatic;
    _contentChangeDynamic = 0;
    _contentChangeStatic = 0;
    if (_allowNegativeContent) {
        return;
    }
    // Snap floating-point round-off residuals to exactly zero. When a store empties,
    // summing nearly-equal in/out fluxes leaves a tiny value (e.g. ±1e-16) that would
    // otherwise show up in the outputs as a tiny, sometimes negative, content.
    if (NearlyZero(_content, PRECISION)) {
        _content = 0;
        return;
    }
    assert(GreaterThanOrEqual(_content, 0, PRECISION));
    if (LessThan(_content, 0, PRECISION)) {
        LogError("Water container {} has negative content ({}).", GetParentBrick()->GetName(), _content);
        _content = 0;
    }
}

void WaterContainer::Reset() {
    _content = _initialState;
    _contentChangeDynamic = 0;
    _contentChangeStatic = 0;
    _inputsBooked = false;
}

double WaterContainer::BookIncomingFluxes(bool asStatic) {
    double amount = SumIncomingFluxes();
    if (asStatic) {
        AddAmountToStaticContentChange(amount);
    } else {
        AddAmountToDynamicContentChange(amount);
    }
    // Flag the amounts as booked so that ApplyConstraints does not count them twice.
    _inputsBooked = true;

    return amount;
}

void WaterContainer::SaveAsInitialState() {
    _initialState = _content;
}

double WaterContainer::SumIncomingFluxes() const {
    double sum = 0;
    for (auto& input : _inputs) {
        sum += input->GetAmount();
    }

    return sum;
}

double WaterContainer::SumIncomingChangeRates() const {
    double rate = 0;
    for (auto& input : _inputs) {
        // Skip the inputs that are not integrated as change rates (forcing, static and
        // instantaneous fluxes deliver an amount, not a rate).
        if (input->IsForcing() || input->IsStatic() || input->IsInstantaneous()) {
            continue;
        }
        double* changeRate = input->GetChangeRatePointer();
        if (changeRate != nullptr) {
            rate += *changeRate;
        }
    }

    return rate;
}

bool WaterContainer::ContentAccessible() const {
    return GetContentWithChanges() > 0;
}

vecDoublePt WaterContainer::GetDynamicContentChanges() {
    return vecDoublePt{&_contentChangeDynamic};
}

double WaterContainer::GetTargetFillingRatio() const {
    assert(GetMaximumCapacity() > 0);
    return std::max(0.0, std::min(1.0, GetContentWithChanges() / GetMaximumCapacity()));
}
