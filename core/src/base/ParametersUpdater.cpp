#include "ParametersUpdater.h"

ParametersUpdater::ParametersUpdater()
    : _active(false),
      _previousDate(0) {}

void ParametersUpdater::AddParameterVariableYearly(ParameterVariableYearly* parameter) {
    _parametersYearly.push_back(parameter);
    _active = true;
}

void ParametersUpdater::AddParameterVariableMonthly(ParameterVariableMonthly* parameter) {
    _parametersMonthly.push_back(parameter);
    _active = true;
}

void ParametersUpdater::AddParameterVariableDates(ParameterVariableDates* parameter) {
    _parametersDates.push_back(parameter);
    _active = true;
}

void ParametersUpdater::DateUpdate(double date) {
    if (!_active) {
        return;
    }

    Time prevDate = GetTimeStructFromMJD(_previousDate);
    Time newDate = GetTimeStructFromMJD(date);

    if (prevDate.year != newDate.year) {
        ChangingYear(newDate.year);
        ChangingMonth(newDate.month);
        ChangingDate(date);
    } else if (prevDate.month != newDate.month) {
        ChangingMonth(newDate.month);
        ChangingDate(date);
    } else {
        ChangingDate(date);
    }

    _previousDate = date;
}

void ParametersUpdater::ChangingYear(int year) {
    for (auto& parameter : _parametersYearly) {
        parameter->UpdateParameter(year);
    }
}

void ParametersUpdater::ChangingMonth(int month) {
    for (auto& parameter : _parametersMonthly) {
        parameter->UpdateParameter(month);
    }
}

void ParametersUpdater::ChangingDate(double date) {
    for (auto& parameter : _parametersDates) {
        parameter->UpdateParameter(date);
    }
}
