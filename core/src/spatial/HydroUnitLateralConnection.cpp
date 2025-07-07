#include "HydroUnitLateralConnection.h"
#include "HydroUnit.h"

#include <utility>

HydroUnitLateralConnection::HydroUnitLateralConnection(HydroUnit* receiver, double fraction, string type)
    : m_receiver(std::move(receiver)),
      m_fraction(fraction),
      m_type(std::move(type)) {}
