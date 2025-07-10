#include "Parameter.h"

Parameter::Parameter(const string& name, float val)
    : _linked(false),
      _name(name),
      _value(val) {}
