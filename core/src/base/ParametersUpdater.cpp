#include "ParametersUpdater.h"

#include "Parameter.h"
#include "ParameterModifier.h"

ParametersUpdater::ParametersUpdater()
    : _active(false),
      _previousDate(0) {}

void ParametersUpdater::AddParameter(Parameter* parameter) {
    if (!parameter || !parameter->HasModifier()) {
        return;
    }

    ParameterModifier* modifier = parameter->GetModifier();

    if (modifier->UpdatesOnYearChange()) {
        _parametersYearly.push_back(parameter);
        _active = true;
    }

    if (modifier->UpdatesOnMonthChange()) {
        _parametersMonthly.push_back(parameter);
        _active = true;
    }

    if (modifier->UpdatesOnDateChange()) {
        _parametersDates.push_back(parameter);
        _active = true;
    }
}

void ParametersUpdater::DateUpdate(double date) {
    if (!_active) {
        return;
    }

    Time prevDate = GetTimeStructFromMJD(_previousDate);
    Time newDate = GetTimeStructFromMJD(date);

    if (prevDate.year != newDate.year) {
        ChangingYear(date);
        ChangingMonth(date);
        ChangingDate(date);
    } else if (prevDate.month != newDate.month) {
        ChangingMonth(date);
        ChangingDate(date);
    } else {
        ChangingDate(date);
    }

    _previousDate = date;
}

void ParametersUpdater::ChangingYear(double date) {
    for (auto& parameter : _parametersYearly) {
        parameter->UpdateFromModifier(date);
    }
}

void ParametersUpdater::ChangingMonth(double date) {
    for (auto& parameter : _parametersMonthly) {
        parameter->UpdateFromModifier(date);
    }
}

void ParametersUpdater::ChangingDate(double date) {
    for (auto& parameter : _parametersDates) {
        parameter->UpdateFromModifier(date);
    }
}
