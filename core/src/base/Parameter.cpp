#include "Parameter.h"

Parameter::Parameter(const string& name, float val)
    : m_linked(false),
      m_name(name),
      m_value(val) {}
