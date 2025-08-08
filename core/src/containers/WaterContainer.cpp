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

bool WaterContainer::IsOk() {
    if (_inputs.empty()) {
        return true;
    }

    for (auto process : GetParentBrick()->GetProcesses()) {
        if (process->GetWaterContainer() == this) {
            return true;
        }
    }
    wxLogError(_("A container of the brick %s has no process attached."), GetParentBrick()->GetName());

    return false;
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
    for (auto process : _parent->GetProcesses()) {
        if (process->GetWaterContainer() != this) {
            continue;
        }
        for (auto flux : process->GetOutputFluxes()) {
            double* changeRate = flux->GetChangeRatePointer();
            if (changeRate == nullptr) {
                // For example when the originating brick has an area = 0.
                continue;
            }
            wxASSERT(changeRate);
            wxASSERT(*changeRate < 1000);
            if (*changeRate < 0) {
                *changeRate = 0;
            } else if (*changeRate > 1000) {
                throw ConceptionIssue(wxString::Format(_("Change rate %f in process %s is too high."),
                    *changeRate, process->GetName()));
            }
            wxASSERT(*changeRate > -EPSILON_D);
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
        wxASSERT(*changeRate > -EPSILON_D);
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
            wxASSERT(*rate > -EPSILON_D);
            wxASSERT(*rate >= 0);
            if (*rate <= EPSILON_D) {
                continue;
            }
            if (std::abs(diff - change) < PRECISION) {
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
                if (_overflow->GetOutputFluxes()[0]->GetChangeRatePointer() != nullptr) {
                    *(_overflow->GetOutputFluxes()[0]->GetChangeRatePointer()) = diff;
                    return;
                }
                throw ShouldNotHappen();
            }
            // Check that it is not only due to forcing
            if (content + inputsStatic > *_capacity) {
                throw ConceptionIssue(
                    _("Forcing is coming directly into a brick with limited capacity and no overflow."));
            }
            // Limit the different rates proportionally
            for (auto rate : incomingRates) {
                wxASSERT(rate != nullptr);
                wxASSERT(*rate < 1000);
                wxASSERT(*rate > -EPSILON_D);
                if (*rate == 0.0) {
                    continue;
                }
                *rate -= diff * std::abs((*rate) / inputs);
            }
        }
    }
}

void WaterContainer::SetOutgoingRatesToZero() {
    for (auto process : _parent->GetProcesses()) {
        if (process->GetWaterContainer() != this) {
            continue;
        }
        for (auto flux : process->GetOutputFluxes()) {
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
    wxASSERT(_content >= -PRECISION);
    if (_content < -PRECISION) {
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

double WaterContainer::SumIncomingFluxes() {
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

double WaterContainer::GetTargetFillingRatio() {
    wxASSERT(GetMaximumCapacity() > 0);
    return wxMax(0.0, wxMin(1.0, GetContentWithChanges() / GetMaximumCapacity()));
}
