#include "Action.h"

#include "ModelHydro.h"

Action::Action()
    : _manager(nullptr),
      _cursor(0),
      _recursive(false) {}

void Action::Reset() {
    _cursor = 0;
}

bool Action::Init() {
    return true;
}

bool Action::Apply(double) {
    return false;
}

bool Action::ApplyIfRecursive(const Time date) {
    for (int month : _recursiveMonths) {
        if (month != date.month) {
            continue;
        }
        for (int day : _recursiveDays) {
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
    for (double storedDate : _sporadicDates) {
        if (date <= storedDate) {
            break;
        }
        index++;
    }

    return index;
}