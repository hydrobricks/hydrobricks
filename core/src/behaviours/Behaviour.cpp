#include "Behaviour.h"

#include "ModelHydro.h"

Behaviour::Behaviour()
    : m_manager(nullptr),
      m_cursor(0) {}

bool Behaviour::Apply(double) {
    return false;
}

void Behaviour::Reset() {
    m_cursor = 0;
}

int Behaviour::GetIndexForInsertion(double date) {
    int index = 0;
    for (double storedDate : m_dates) {
        if (date <= storedDate) {
            break;
        }
        index++;
    }

    return index;
}