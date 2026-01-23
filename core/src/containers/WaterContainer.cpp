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
      _parent(brick),
      _overflow(nullptr) {}

bool WaterContainer::IsValid() const {
    for (int i = 0; i < GetParentBrick()->GetProcessCount(); ++i) {
        auto process = GetParentBrick()->GetProcess(i);
        if (process->GetWaterContainer() == this) {
            return true;
        }
    }
    wxLogError(_("A container of the brick %s has no process attached."), GetParentBrick()->GetName());

    return false;
}

void WaterContainer::Validate() const {
    if (!IsValid()) {
        throw ModelConfigError(wxString::Format(_("A container of the brick %s has no process attached."), GetParentBrick()->GetName()));
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

void WaterContainer::ApplyConstraints(double timeStep) {
    if (_infiniteStorage) return;

    // Get outgoing change rates
    vecDoublePt outgoingRates;
    double outputs = 0;
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
            wxASSERT(changeRate);
            wxASSERT(*changeRate < 10000);
            if (*changeRate < 0) {
                *changeRate = 0;
            } else if (*changeRate > 10000) {
                throw RuntimeError(
                    wxString::Format(_("Change rate %f in process %s is too high."), *changeRate, process->GetName()));
            }
            wxASSERT(GreaterThanOrEqual(*changeRate, 0, EPSILON_D));
            outgoingRates.push_back(changeRate);
            outputs += *changeRate;
        }
    }

    // Get incoming change rates
    vecDoublePt incomingRates;
    double inputs = 0;
    double inputsStatic = 0;
    for (auto& input : _inputs) {
        if (input->IsInstantaneous()) {
            inputsStatic += dynamic_cast<FluxToBrickInstantaneous*>(input)->GetRealAmount();
            continue;
        }
        if (input->IsForcing() || input->IsStatic()) {
            inputsStatic += input->GetAmount();
            continue;
        }
        double* changeRate = input->GetChangeRatePointer();
        if (changeRate == nullptr) {
            // For example when the originating brick has an area = 0.
            continue;
        }
        wxASSERT(changeRate);
        wxASSERT(*changeRate < 1000);
        if (*changeRate < 0) {
            *changeRate = 0;
        }
        wxASSERT(GreaterThanOrEqual(*changeRate, 0, EPSILON_D));
        incomingRates.push_back(changeRate);
        inputs += *changeRate;
    }

    double change = inputs - outputs;
    double content = GetContentWithDynamicChanges();

    // Avoid negative content
    if (change < 0 && content + inputsStatic + change * timeStep < 0) {
        double diff = (content + inputsStatic + change * timeStep) / timeStep;
        // Limit the different rates proportionally
        for (auto rate : outgoingRates) {
            wxASSERT(rate != nullptr);
            wxASSERT(*rate < 1000);
            wxASSERT(GreaterThanOrEqual(*rate, 0, EPSILON_D));
            wxASSERT(*rate >= 0);
            if (NearlyZero(*rate, EPSILON_D)) {
                continue;
            }
            if (NearlyEqual(diff, change, PRECISION)) {
                *rate = 0;
                continue;
            }
            *rate += diff * std::abs((*rate) / outputs);
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
                throw ShouldNotHappen(_("WaterContainer::ApplyConstraints - Overflow exists but has no change rate pointer"));
            }
            // Check that it is not only due to forcing
            if (content + inputsStatic > *_capacity) {
                throw ModelConfigError(
                    _("Forcing is coming directly into a brick with limited capacity and no overflow."));
            }
            // Limit the different rates proportionally
            for (auto rate : incomingRates) {
                wxASSERT(rate != nullptr);
                wxASSERT(*rate < 1000);
                wxASSERT(GreaterThanOrEqual(*rate, 0, EPSILON_D));
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
            wxASSERT(changeRate);
            *changeRate = 0;
        }
    }
}

void WaterContainer::Finalize() {
    if (_infiniteStorage) return;
    _content += _contentChangeDynamic + _contentChangeStatic;
    _contentChangeDynamic = 0;
    _contentChangeStatic = 0;
    wxASSERT(GreaterThanOrEqual(_content, 0, PRECISION));
    if (LessThan(_content, 0, PRECISION)) {
        wxLogError(_("Water container %s has negative content (%f)."), GetParentBrick()->GetName(), _content);
        _content = 0;
    }
}

void WaterContainer::Reset() {
    _content = _initialState;
    _contentChangeDynamic = 0;
    _contentChangeStatic = 0;
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

bool WaterContainer::ContentAccessible() const {
    return GetContentWithChanges() > 0;
}

vecDoublePt WaterContainer::GetDynamicContentChanges() {
    return vecDoublePt{&_contentChangeDynamic};
}

double WaterContainer::GetTargetFillingRatio() const {
    wxASSERT(GetMaximumCapacity() > 0);
    return wxMax(0.0, wxMin(1.0, GetContentWithChanges() / GetMaximumCapacity()));
}
