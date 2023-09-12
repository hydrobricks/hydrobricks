#ifndef HYDROBRICKS_HYDRO_UNIT_PROPERTY_H
#define HYDROBRICKS_HYDRO_UNIT_PROPERTY_H

#include "Includes.h"

class HydroUnitProperty : public wxObject {
  public:
    HydroUnitProperty();

    HydroUnitProperty(string name, double value, string unit = "");

    HydroUnitProperty(string name, string valueString, string unit = "");

    ~HydroUnitProperty() override = default;

    double GetValue(const string& unit = "") const;

    string GetValueString() const;

    string GetName() {
        return m_name;
    }

  protected:
  private:
    string m_name;
    string m_unit;
    double m_value;
    string m_valueString;
};

#endif  // HYDROBRICKS_HYDRO_UNIT_PROPERTY_H
