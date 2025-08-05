#ifndef HYDROBRICKS_HYDRO_UNIT_LATERAL_CONNECTION_H
#define HYDROBRICKS_HYDRO_UNIT_LATERAL_CONNECTION_H

#include "Includes.h"

class HydroUnit;

class HydroUnitLateralConnection : public wxObject {
  public:
    HydroUnitLateralConnection(HydroUnit* receiver, double fraction, string type = "");

    ~HydroUnitLateralConnection() override = default;

    /**
     * Get the hydro unit that receives the flow from this lateral connection.
     *
     * @return
     */
    HydroUnit* GetReceiver() const {
        return _receiver;
    }

    /**
     * Get the fraction of the flow that is transferred to the receiver hydro unit.
     *
     * @return The fraction of the flow.
     */
    double GetFraction() const {
        return _fraction;
    }

    /**
     * Get the type of the lateral connection.
     *
     * @return The type of the lateral connection.
     */
    string GetType() const {
        return _type;
    }

  private:
    HydroUnit* _receiver;
    double _fraction;
    string _type;
};

#endif  // HYDROBRICKS_HYDRO_UNIT_LATERAL_CONNECTION_H
