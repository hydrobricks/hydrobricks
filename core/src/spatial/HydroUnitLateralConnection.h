#ifndef HYDROBRICKS_HYDRO_UNIT_LATERAL_CONNECTION_H
#define HYDROBRICKS_HYDRO_UNIT_LATERAL_CONNECTION_H

#include "Includes.h"

class HydroUnit;

class HydroUnitLateralConnection : public wxObject {
  public:
    HydroUnitLateralConnection(HydroUnit* receiver, double fraction, string type = "");

    ~HydroUnitLateralConnection() override = default;

  private:
    HydroUnit* m_receiver;
    double m_fraction;
    string m_type;
};

#endif  // HYDROBRICKS_HYDRO_UNIT_LATERAL_CONNECTION_H
