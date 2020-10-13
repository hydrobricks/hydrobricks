
#include "ParametersUpdater.h"

ParametersUpdater::ParametersUpdater()
    : m_previousDate(1, wxDateTime::Jan, 0)
{}

void ParametersUpdater::AddParameterVariableYearly(ParameterVariableYearly* parameter) {
    m_parametersYearly.push_back(parameter);
}

void ParametersUpdater::AddParameterVariableMonthly(ParameterVariableMonthly* parameter) {
    m_parametersMonthly.push_back(parameter);
}

void ParametersUpdater::AddParameterVariableDates(ParameterVariableDates* parameter) {
    m_parametersDates.push_back(parameter);
}

void ParametersUpdater::DateUpdate(const wxDateTime &date) {
    if (m_previousDate.GetYear() != date.GetYear()) {
        ChangingYear(date.GetYear());
        ChangingMonth(date.GetMonth());
        ChangingDate(date.GetJulianDayNumber());
    } else if (m_previousDate.GetMonth() != date.GetMonth()) {
        ChangingMonth(date.GetMonth());
        ChangingDate(date.GetJulianDayNumber());
    } else {
        ChangingDate(date.GetJulianDayNumber());
    }

    m_previousDate = date;
}

void ParametersUpdater::ChangingYear(int year) {
    for (auto & parameter : m_parametersYearly) {
        parameter->UpdateParameter(year);
    }
}

void ParametersUpdater::ChangingMonth(wxDateTime::Month month) {
    for (auto & parameter : m_parametersMonthly) {
        parameter->UpdateParameter(month);
    }
}

void ParametersUpdater::ChangingDate(double date) {
    for (auto & parameter : m_parametersDates) {
        parameter->UpdateParameter(date);
    }
}