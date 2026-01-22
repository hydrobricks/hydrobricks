#include "Parameter.h"

Parameter::Parameter(const string& name, float val)
    : _name(name),
      _value(val),
      _modifier(),
      _hasModifier(false) {}

Parameter::~Parameter() = default;

bool Parameter::UpdateFromModifier(double date) {
    if (!_hasModifier) {
        return false;
    }

    float newValue = _modifier.UpdateValue(date);
    if (std::isnan(newValue)) {
        return false;
    }

    _value = newValue;
    return true;
}
