#include "HydroUnitProperty.h"

#include <utility>

HydroUnitProperty::HydroUnitProperty()
    : m_value(NAN_D) {}

HydroUnitProperty::HydroUnitProperty(string name, double value, string unit)
    : m_name(std::move(name)),
      m_unit(std::move(unit)),
      m_value(value) {}

HydroUnitProperty::HydroUnitProperty(string name, string valueString, string unit)
    : m_name(std::move(name)),
      m_unit(std::move(unit)),
      m_value(NAN_D),
      m_valueString(std::move(valueString)) {}

double HydroUnitProperty::GetValue(const string& unit) const {
    if (m_unit == unit) {
        return m_value;
    } else if (m_unit == "degrees") {
        if (unit == "radians") {
            return m_value * M_PI / 180.0;
        } else if (unit == "percent") {
            return 100 * tan(m_value * M_PI / 180.0);
        } else if (unit == "m/m") {
            return tan(m_value * M_PI / 180.0);
        }
    } else if (m_unit == "m2" || m_unit == "m^2") {
        if (unit == "ha") {
            return m_value / 10000.0;
        } else if (unit == "km2") {
            return m_value / 1000000.0;
        }
    }
    wxLogError(_("The unit '%s' is not supported for the property '%s'."), unit, m_name);

    return NAN_D;
}

string HydroUnitProperty::GetValueString() const {
    if (m_valueString.empty()) {
        wxLogError(_("The value (string) for the property '%s' is empty."), m_name);
    }

    return m_valueString;
}