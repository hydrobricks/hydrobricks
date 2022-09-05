#include "Parameter.h"

Parameter::Parameter(const wxString &name, float val)
    : m_linked(false),
      m_name(name),
      m_value(val)
{}