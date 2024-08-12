#include "WaterContainer.h"

#include "Brick.h"
#include "FluxToBrickInstantaneous.h"

WaterContainer::WaterContainer(Brick* brick)
    : m_content(0),
      m_contentChangeDynamic(0),
      m_contentChangeStatic(0),
      m_initialState(0),
      m_capacity(nullptr),
      m_infiniteStorage(false),
      m_parent(brick),
      m_overflow(nullptr) {}

bool WaterContainer::IsOk() {
    if (m_inputs.empty()) {
        return true;
    }

    for (auto process : GetParentBrick()->GetProcesses()) {
        if (process->GetWaterContainer() == this) {
            return true;
        }
    }
    wxLogError(_("The water container of the brick %s has no process attached."), GetParentBrick()->GetName());

    return false;
}

void WaterContainer::SubtractAmountFromDynamicContentChange(double change) {
    if (m_infiniteStorage) return;
    m_contentChangeDynamic -= change;
}

void WaterContainer::AddAmountToDynamicContentChange(double change) {
    if (m_infiniteStorage) return;
    m_contentChangeDynamic += change;
}

void WaterContainer::AddAmountToStaticContentChange(double change) {
    if (m_infiniteStorage) return;
    m_contentChangeStatic += change;
}

void WaterContainer::ApplyConstraints(double timeStep) {
    if (m_infiniteStorage) return;

    // Get outgoing change rates
    vecDoublePt outgoingRates;
    double outputs = 0;
    for (auto process : m_parent->GetProcesses()) {
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
    for (auto& input : m_inputs) {
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
        if (content + inputsStatic + change * timeStep > *m_capacity) {
            double diff = (content + inputsStatic + change * timeStep - *m_capacity) / timeStep;
            // If it has an overflow, use it
            if (HasOverflow()) {
                if (m_overflow->GetOutputFluxes()[0]->GetChangeRatePointer() != nullptr) {
                    *(m_overflow->GetOutputFluxes()[0]->GetChangeRatePointer()) = diff;
                    return;
                }
                throw ShouldNotHappen();
            }
            // Check that it is not only due to forcing
            if (content + inputsStatic > *m_capacity) {
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
    for (auto process : m_parent->GetProcesses()) {
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
    if (m_infiniteStorage) return;
    m_content += m_contentChangeDynamic + m_contentChangeStatic;
    m_contentChangeDynamic = 0;
    m_contentChangeStatic = 0;
    wxASSERT(m_content >= -PRECISION);
}

void WaterContainer::Reset() {
    m_content = m_initialState;
    m_contentChangeDynamic = 0;
    m_contentChangeStatic = 0;
}

void WaterContainer::SaveAsInitialState() {
    m_initialState = m_content;
}

void WaterContainer::SetInitialState(double content) {
    m_content = content;
    m_initialState = content;
}

double WaterContainer::SumIncomingFluxes() {
    double sum = 0;
    for (auto& input : m_inputs) {
        sum += input->GetAmount();
    }

    return sum;
}

bool WaterContainer::ContentAccessible() const {
    return GetContentWithChanges() > 0;
}

vecDoublePt WaterContainer::GetDynamicContentChanges() {
    return vecDoublePt{&m_contentChangeDynamic};
}

double WaterContainer::GetTargetFillingRatio() {
    wxASSERT(GetMaximumCapacity() > 0);
    return wxMax(0.0, wxMin(1.0, GetContentWithChanges() / GetMaximumCapacity()));
}
