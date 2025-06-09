#include "Action.h"

#include "ModelHydro.h"

Action::Action()
    : m_manager(nullptr),
      m_cursor(0),
      m_recursive(false) {}

void Action::Reset() {
    m_cursor = 0;
}

bool Action::Init() {
    return true;
}

bool Action::Apply(double) {
    return false;
}

bool Action::ApplyIfRecursive(const Time date) {
    for (int month : m_recursiveMonths) {
        if (month != date.month) {
            continue;
        }
        for (int day : m_recursiveDays) {
            if (day != date.day) {
                continue;
            }

            return Apply();
        }
    }

    return true;
}

int Action::GetIndexForInsertion(double date) {
    int index = 0;
    for (double storedDate : m_sporadicDates) {
        if (date <= storedDate) {
            break;
        }
        index++;
    }

    return index;
}