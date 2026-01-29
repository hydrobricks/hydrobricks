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

bool Parameter::IsValid() const {
    // Check that parameter has a name
    if (_name.empty()) {
        wxLogError(_("Parameter: Name is empty."));
        return false;
    }

    // Check that parameter has a valid value (not NaN)
    if (std::isnan(_value)) {
        wxLogError(_("Parameter '%s': Value is NaN."), _name);
        return false;
    }

    return true;
}

void Parameter::Validate() const {
    if (!IsValid()) {
        wxString msg = wxString::Format(_("Parameter validation failed. Name: '%s', Value: %f"), _name, _value);
        throw ModelConfigError(msg);
    }
}
