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

std::vector<double*> Brick::GetIterableValues() {
    return std::vector<double*> {&m_contentNext};
}

std::vector<double*> Brick::GetIterableValuesFromProcesses() {
    std::vector<double*> values;
    for (auto const &process: m_processes) {
        std::vector<double*> processValues = process->GetIterableValues();

        if (processValues.empty()) {
            continue;
        }
        for (auto const &value : processValues) {
            values.push_back(value);
        }
    }

    return values;
}

std::vector<double*> Brick::GetIterableValuesFromOutgoingFluxes() {
    std::vector<double*> values;
    for (auto const &flux: m_outputs) {
        std::vector<double*> fluxValues = flux->GetIterableValues();

        if (fluxValues.empty()) {
            continue;
        }
        for (auto const &value : fluxValues) {
            values.push_back(value);
        }
    }

    return values;
}