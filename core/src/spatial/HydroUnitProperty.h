#ifndef HYDROBRICKS_HYDRO_UNIT_PROPERTY_H
#define HYDROBRICKS_HYDRO_UNIT_PROPERTY_H

#include "Includes.h"

class HydroUnitProperty : public wxObject {
  public:
    HydroUnitProperty();

    HydroUnitProperty(string name, double value, string unit = "");

    HydroUnitProperty(string name, string valueString, string unit = "");

    ~HydroUnitProperty() override = default;

    /**
     * Get the value of the property.
     *
     * @param unit The unit to convert the value to. If empty, the original unit is used.
     * @return The value of the property.
     */
    double GetValue(const string& unit = "") const;

    /**
     * Get the value of the property as a string.
     *
     * @return The value of the property as a string.
     */
    string GetValueString() const;

    /**
     * Get the name of the property.
     *
     * @return The name of the property.
     */
    string GetName() {
        return _name;
    }

  private:
    string _name;
    string _unit;
    double _value;
    string _valueString;
};

#endif  // HYDROBRICKS_HYDRO_UNIT_PROPERTY_H
