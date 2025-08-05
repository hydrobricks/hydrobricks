#include "ParameterVariable.h"

ParameterVariable::ParameterVariable(const string& name)
    : Parameter(name) {}

/*
 * Class ParameterVariableYearly
 */

ParameterVariableYearly::ParameterVariableYearly(const string& name)
    : ParameterVariable(name) {}

bool ParameterVariableYearly::SetValues(int yearStart, int yearEnd, const vecFloat& values) {
    _values = values;
    _reference.reserve(yearEnd - yearStart + 1);
    for (int i = yearStart; i <= yearEnd; ++i) {
        _reference.push_back(i);
    }

    if (_values.size() != _reference.size()) {
        wxLogError(_("The length of the variable parameters values and the number of years do not match."));
        return false;
    }

    return true;
}

bool ParameterVariableYearly::UpdateParameter(int year) {
    wxASSERT(!_reference.empty());

    int i = Find(&_reference.front(), &_reference.back(), year);

    if (i < 0) {
        wxLogError(_("The given year was not found in the reference years of the parameter."));
        _value = NAN_F;
        return false;
    }

    _value = _values[i];

    return false;
}

/*
 * Class ParameterVariableMonthly
 */

ParameterVariableMonthly::ParameterVariableMonthly(const string& name)
    : ParameterVariable(name) {}

bool ParameterVariableMonthly::SetValues(const vecFloat& values) {
    _values = values;

    if (_values.size() != 12) {
        wxLogError(_("The length of the variable parameters values and the number of months do not match."));
        return false;
    }

    return true;
}

bool ParameterVariableMonthly::UpdateParameter(int month) {
    wxASSERT(month > 0);
    wxASSERT(month <= 12);

    _value = _values[month - 1];

    return true;
}

/*
 * Class ParameterVariableDates
 */

ParameterVariableDates::ParameterVariableDates(const string& name)
    : ParameterVariable(name) {}

bool ParameterVariableDates::SetTimeAndValues(const vecDouble& time, const vecFloat& values) {
    _reference = time;
    _values = values;

    if (_values.size() != _reference.size()) {
        wxLogError(_("The length of the variable parameters values and the time do not match."));
        return false;
    }

    return true;
}

bool ParameterVariableDates::UpdateParameter(double timeReference) {
    wxASSERT(!_reference.empty());

    int i = Find(&_reference.front(), &_reference.back(), timeReference);

    if (i < 0) {
        wxLogError(_("The given time was not found in the reference time array of the parameter."));
        _value = NAN_F;
        return false;
    }

    _value = _values[i];

    return true;
}
