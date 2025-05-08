#include "Action.h"

#include "ModelHydro.h"

Action::Action()
    : m_manager(nullptr),
      m_cursor(0) {}

bool Action::Apply(double) {
    return false;
}

void Action::Reset() {
    m_cursor = 0;
}

int Action::GetIndexForInsertion(double date) {
    int index = 0;
    for (double storedDate : m_dates) {
        if (date <= storedDate) {
            break;
        }
        index++;
    }

    return index;
}