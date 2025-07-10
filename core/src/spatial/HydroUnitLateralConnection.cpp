#include "HydroUnitLateralConnection.h"
#include "HydroUnit.h"

#include <utility>

HydroUnitLateralConnection::HydroUnitLateralConnection(HydroUnit* receiver, double fraction, string type)
    : _receiver(receiver),
      _fraction(fraction),
      _type(std::move(type)) {}
