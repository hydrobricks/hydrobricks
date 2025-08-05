#include "HydroUnitLateralConnection.h"

#include <utility>

#include "HydroUnit.h"

HydroUnitLateralConnection::HydroUnitLateralConnection(HydroUnit* receiver, double fraction, string type)
    : _receiver(receiver),
      _fraction(fraction),
      _type(std::move(type)) {}
