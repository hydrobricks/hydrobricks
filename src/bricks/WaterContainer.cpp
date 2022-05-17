#include "WaterContainer.h"
#include "Brick.h"

WaterContainer::WaterContainer(Brick* brick)
    : m_content(0),
      m_contentChange(0),
      m_capacity(nullptr),
      m_parent(brick),
      m_overflow(nullptr)
{}

void WaterContainer::SubtractAmount(double change) {
    m_contentChange -= change;
}

void WaterContainer::AddAmount(double change) {
    m_contentChange += change;
}

void WaterContainer::ApplyConstraints(double timeStep) {
    // Get outgoing change rates
    vecDoublePt outgoingRates;
    double outputs = 0;
    for (auto process : m_parent->GetProcesses()) {
        for (auto flux : process->GetOutputFluxes()) {
            double* changeRate = flux->GetChangeRatePointer();
            wxASSERT(changeRate != nullptr);
            outgoingRates.push_back(changeRate);
            outputs += *changeRate;
        }
    }

    // Get incoming change rates
    vecDoublePt incomingRates;
    double inputs = 0;
    double inputsStatic = 0;
    for (auto & input : m_parent->GetInputs()) {
        if (input->IsForcing()) {
            inputsStatic += input->GetAmount();
        } else if (input->IsStatic()) {
            inputsStatic += input->GetAmount();
        } else {
            double* changeRate = input->GetChangeRatePointer();
            wxASSERT(changeRate != nullptr);
            incomingRates.push_back(changeRate);
            inputs += *changeRate;
        }
    }

    double change = inputs - outputs;

    // Avoid negative content
    if (change < 0 && GetContentWithChanges() + inputsStatic + change * timeStep < 0) {
        double diff = (GetContentWithChanges() + inputsStatic + change * timeStep) / timeStep;
        // Limit the different rates proportionally
        for (auto rate :outgoingRates) {
            if (*rate == 0) {
                continue;
            }
            *rate += diff * std::abs((*rate) / outputs);
        }
    }

    // Enforce maximum capacity
    if (HasMaximumCapacity()) {
        if (GetContentWithChanges() + inputsStatic + change * timeStep > *m_capacity) {
            double diff = (GetContentWithChanges() + inputsStatic + change * timeStep - *m_capacity) / timeStep;
            // If it has an overflow, use it
            if (HasOverflow()) {
                *(m_overflow->GetOutputFluxes()[0]->GetChangeRatePointer()) = diff;
                return;
            }
            // Check that it is not only due to forcing
            if (GetContentWithChanges() + inputsStatic > *m_capacity) {
                throw ConceptionIssue(_("Forcing is coming directly into a brick with limited capacity and no overflow."));
            }
            // Limit the different rates proportionally
            for (auto rate :incomingRates) {
                if (*rate == 0) {
                    continue;
                }
                *rate -= diff * std::abs((*rate) / inputs);
            }
        }
    }
}

void WaterContainer::Finalize() {
    m_content += m_contentChange;
    m_contentChange = 0;
    wxASSERT(m_content >= 0);
}

vecDoublePt WaterContainer::GetStateVariableChanges() {
    return vecDoublePt {&m_contentChange};
}