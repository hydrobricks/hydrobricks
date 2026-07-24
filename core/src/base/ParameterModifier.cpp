#include "ParameterModifier.h"

bool ParameterModifier::SetYearlyValues(int yearStart, int yearEnd, const vecFloat& values) {
    if (_type != ParameterModifierType::Yearly) {
        throw ModelConfigError("ParameterModifier::SetYearlyValues - Modifier type is not Yearly.");
    }

    _values = values;
    _dates.clear();
    _dates.reserve(yearEnd - yearStart + 1);
    for (int i = yearStart; i <= yearEnd; ++i) {
        _dates.push_back(static_cast<double>(i));
    }

    if (_values.size() != _dates.size()) {
        LogError("The length of the variable parameter values and the number of years do not match.");
        return false;
    }

    return true;
}

bool ParameterModifier::SetMonthlyValues(const vecFloat& values) {
    if (_type != ParameterModifierType::Monthly) {
        throw ModelConfigError("ParameterModifier::SetMonthlyValues - Modifier type is not Monthly.");
    }

    _values = values;
    _dates.clear();

    if (_values.size() != 12) {
        LogError("The length of the variable parameter values must be 12 (one per month).");
        return false;
    }

    return true;
}

bool ParameterModifier::SetDatesAndValues(const vecDouble& dates, const vecFloat& values) {
    if (_type != ParameterModifierType::Dates) {
        throw ModelConfigError("ParameterModifier::SetDatesAndValues - Modifier type is not Dates.");
    }

    _dates = dates;
    _values = values;

    if (_values.size() != _dates.size()) {
        LogError("The length of the variable parameter values and the dates do not match.");
        return false;
    }

    return true;
}

float ParameterModifier::UpdateValue(double date) {
    switch (_type) {
        case ParameterModifierType::Yearly: {
            assert(!_dates.empty());
            Time timeStruct = GetTimeStructFromMJD(date);
            int year = timeStruct.year;

            int i = Find(&_dates.front(), &_dates.back(), static_cast<double>(year));

            if (i < 0) {
                LogError("The given year was not found in the reference years of the parameter.");
                return NAN_F;
            }

            return _values[i];
        }

        case ParameterModifierType::Monthly: {
            Time timeStruct = GetTimeStructFromMJD(date);
            int month = timeStruct.month;

            assert(month > 0);
            assert(month <= 12);

            return _values[month - 1];
        }

        case ParameterModifierType::Dates: {
            assert(!_dates.empty());

            int i = Find(&_dates.front(), &_dates.back(), date);

            if (i < 0) {
                LogError("The given date was not found in the reference dates of the parameter.");
                return NAN_F;
            }

            return _values[i];
        }
    }

    return NAN_F;
}
