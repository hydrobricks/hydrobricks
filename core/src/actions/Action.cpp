#include "Action.h"

#include "ModelHydro.h"

Action::Action()
    : _manager(nullptr),
      _cursor(0),
      _recursive(false) {}

bool Action::Init() {
    return true;
}

void Action::ResetCursor() {
    _cursor = 0;
}

void Action::AddRecursiveDate(int month, int day) {
    // Check that the month is valid.
    if (month < 1 || month > 12) {
        month = 1;
        wxLogError("Invalid month: %d", month);
    }

    // Check that the day is valid for the given month.
    day = std::min(day, constants::daysInMonth[month - 1]);
    day = std::max(day, 1);

    _recursive = true;
    _recursiveMonths.push_back(month);
    _recursiveDays.push_back(day);
}

void Action::Reset() {
    throw ConceptionIssue(_("Reset action not implemented for child action class."));
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

double Action::CheckLandCoverAreaFraction(const string& name, int id, double fraction, double unitArea, double lcArea) {
    // If close to 0, set to 0.
    if (std::abs(fraction) < PRECISION) {
        return 0.0;
    }

    // If close to 1, set to 1.
    if (std::abs(1 - fraction) < PRECISION) {
        return 1.0;
    }

    // If the fraction is not in the range [0, 1], raise an error.
    if ((fraction < 0) || (fraction > 1)) {
        wxLogError(_("The given fraction (%f) for '%s' is not in the allowed range [0 .. 1]"), fraction, name);
        wxLogError(_("The unit area is %g, and the land cover area to assign is %g."), unitArea, lcArea);
        wxLogError(_("Failed to set the '%s' area fraction for hydro unit %d."), name, id);
        throw ConceptionIssue(wxString::Format(_("The fraction (%f) is not in the range [0 .. 1]"), fraction));
    }

    return fraction;
}