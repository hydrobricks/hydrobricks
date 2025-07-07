#include "HydroUnitProperty.h"

#include <utility>

HydroUnitProperty::HydroUnitProperty()
    : _value(NAN_D) {}

HydroUnitProperty::HydroUnitProperty(string name, double value, string unit)
    : _name(std::move(name)),
      _unit(std::move(unit)),
      _value(value) {}

HydroUnitProperty::HydroUnitProperty(string name, string valueString, string unit)
    : _name(std::move(name)),
      _unit(std::move(unit)),
      _value(NAN_D),
      _valueString(std::move(valueString)) {}

double HydroUnitProperty::GetValue(const string& unit) const {
    if (_unit == unit) {
        return _value;
    } else if (_unit == "degrees") {
        if (unit == "radians") {
            return _value * M_PI / 180.0;
        } else if (unit == "percent") {
            return 100 * tan(_value * M_PI / 180.0);
        } else if (unit == "m/m") {
            return tan(_value * M_PI / 180.0);
        }
    } else if (_unit == "m2" || _unit == "m^2") {
        if (unit == "ha") {
            return _value / 10000.0;
        } else if (unit == "km2") {
            return _value / 1000000.0;
        }
    }
    wxLogError(_("The unit '%s' is not supported for the property '%s'."), unit, _name);

    return NAN_D;
}

string HydroUnitProperty::GetValueString() const {
    if (_valueString.empty()) {
        wxLogError(_("The value (string) for the property '%s' is empty."), _name);
    }

    return _valueString;
}