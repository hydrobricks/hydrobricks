#include "ParametersUpdater.h"

ParametersUpdater::ParametersUpdater()
    : m_previousDate(0) {}

void ParametersUpdater::AddParameterVariableYearly(ParameterVariableYearly* parameter) {
    m_parametersYearly.push_back(parameter);
}

void ParametersUpdater::AddParameterVariableMonthly(ParameterVariableMonthly* parameter) {
    m_parametersMonthly.push_back(parameter);
}

void ParametersUpdater::AddParameterVariableDates(ParameterVariableDates* parameter) {
    m_parametersDates.push_back(parameter);
}

void ParametersUpdater::DateUpdate(double date) {
    Time prevDate = GetTimeStructFromMJD(m_previousDate);
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

    m_previousDate = date;
}

void ParametersUpdater::ChangingYear(int year) {
    for (auto& parameter : m_parametersYearly) {
        parameter->UpdateParameter(year);
    }
}

void ParametersUpdater::ChangingMonth(int month) {
    for (auto& parameter : m_parametersMonthly) {
        parameter->UpdateParameter(month);
    }
}

void ParametersUpdater::ChangingDate(double date) {
    for (auto& parameter : m_parametersDates) {
        parameter->UpdateParameter(date);
    }
}
