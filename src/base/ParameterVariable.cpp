#include "ParameterVariable.h"

ParameterVariable::ParameterVariable(const wxString &name, float value)
    : Parameter(name, value)
{}


/*
 * Class ParameterVariableYearly
 */

ParameterVariableYearly::ParameterVariableYearly(const wxString &name, float value)
    : ParameterVariable(name, value)
{}

bool ParameterVariableYearly::SetValues(int yearStart, int yearEnd, const vecFloat& values) {
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

bool ParameterVariableYearly::UpdateParameter(int year) {
    wxASSERT(!m_reference.empty());

    int i = Find(&m_reference.front(), &m_reference.back(), year);

    if (i < 0) {
        wxLogError(_("The given year was not found in the reference years of the parameter."));
        m_value = NAN_F;
        return false;
    }

    m_value = m_values[i];

    return false;
}


/*
 * Class ParameterVariableMonthly
 */

ParameterVariableMonthly::ParameterVariableMonthly(const wxString &name, float value)
    : ParameterVariable(name, value)
{}

bool ParameterVariableMonthly::SetValues(const vecFloat& values) {
    m_values = values;

    if (m_values.size() != 12) {
        wxLogError(_("The length of the variable parameters values and the number of months do not match."));
        return false;
    }

    return true;
}

bool ParameterVariableMonthly::UpdateParameter(wxDateTime::Month month) {
    wxASSERT(month != wxDateTime::Inv_Month);

    m_value = m_values[month];

    return true;
}


/*
 * Class ParameterVariableDates
 */

ParameterVariableDates::ParameterVariableDates(const wxString &name, float value)
    : ParameterVariable(name, value)
{}

bool ParameterVariableDates::SetTimeAndValues(const vecDouble& time, const vecFloat& values) {
    m_reference = time;
    m_values = values;

    if (m_values.size() != m_reference.size()) {
        wxLogError(_("The length of the variable parameters values and the time do not match."));
        return false;
    }

    return true;
}

bool ParameterVariableDates::UpdateParameter(double timeReference) {
    wxASSERT(!m_reference.empty());

    int i = Find(&m_reference.front(), &m_reference.back(), timeReference);

    if (i < 0) {
        wxLogError(_("The given time was not found in the reference time array of the parameter."));
        m_value = NAN_F;
        return false;
    }

    m_value = m_values[i];

    return true;
}