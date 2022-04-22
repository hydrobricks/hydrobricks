#include "Brick.h"

#include "HydroUnit.h"

Brick::Brick(HydroUnit *hydroUnit)
    : m_contentPrev(0),
      m_contentNext(0),
      m_hydroUnit(hydroUnit)
{
    if (hydroUnit) {
        hydroUnit->AddBrick(this);
    }
}

void Brick::Finalize() {
    m_contentPrev = m_contentNext;
}

std::vector<double*> Brick::GetIterableElements() {
    return std::vector<double*> {&m_contentNext};
}

std::vector<double*> Brick::GetIterableElementsFromProcesses() {
    std::vector<double*> elements;
    for (auto const &process: m_processes) {
        std::vector<double*> processElements = process->GetIterableElements();

        if (processElements.empty()) {
            continue;
        }
        for (auto const &element: processElements) {
            elements.push_back(element);
        }
    }

    return elements;
}

std::vector<double*> Brick::GetIterableElementsFromOutgoingFluxes() {
    std::vector<double*> elements;
    for (auto const &flux: m_outputs) {
        std::vector<double*> fluxElements = flux->GetIterableElements();

        if (fluxElements.empty()) {
            continue;
        }
        for (auto const &element: fluxElements) {
            elements.push_back(element);
        }
    }

    return elements;
}