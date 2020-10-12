
#include "ParameterVariable.h"

ParameterVariable::ParameterVariable()
    : Parameter()
{}


/*
 * Class ParameterVariableYearly
 */

ParameterVariableYearly::ParameterVariableYearly()
    : ParameterVariable()
{}

bool ParameterVariableYearly::SetValues(int yearStart, int yearEnd, const std::vector<float>& values) {
    m_values = values;
    m_reference.reserve(yearEnd - yearStart + 1);
    for (int i = yearStart; i <= yearEnd; ++i) {
        m_reference.push_back(i);
    }

    if (m_values.size() != m_reference.size()) {
        wxLogError(_("The length of the variable parameters values and the number of years do not match."));
        return false;
    }

    return true;
}

void ParameterVariableYearly::UpdateParameter(int year) {
    wxASSERT(!m_reference.empty());

    int i = Find(&m_reference.front(), &m_reference.back(), year);

    if (i < 0) {
        wxLogError(_("The given year was not found in the reference years of the parameter."));
        return;
    }

    m_value = m_values[i];
}


/*
 * Class ParameterVariableMonthly
 */

ParameterVariableMonthly::ParameterVariableMonthly()
    : ParameterVariable()
{}

bool ParameterVariableMonthly::SetValues(const std::vector<float>& values) {
    m_values = values;

    if (m_values.size() != 12) {
        wxLogError(_("The length of the variable parameters values and the number of months do not match."));
        return false;
    }

    return true;
}

void ParameterVariableMonthly::UpdateParameter(wxDateTime::Month month) {
    wxASSERT(month != wxDateTime::Inv_Month);

    m_value = m_values[month + 1];
}


/*
 * Class ParameterVariableDates
 */

ParameterVariableDates::ParameterVariableDates()
    : ParameterVariable()
{}

bool ParameterVariableDates::SetTimeAndValues(const std::vector<float>& time, const std::vector<float>& values) {
    m_reference = time;
    m_values = values;

    if (m_values.size() != m_reference.size()) {
        wxLogError(_("The length of the variable parameters values and the time do not match."));
        return false;
    }

    return true;
}

void ParameterVariableDates::UpdateParameter(float timeReference) {
    wxASSERT(!m_reference.empty());

    int i = Find(&m_reference.front(), &m_reference.back(), timeReference);

    if (i < 0) {
        wxLogError(_("The given time was not found in the reference time array of the parameter."));
        return;
    }

    m_value = m_values[i];
}