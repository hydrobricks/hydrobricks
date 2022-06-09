#include "Flux.h"

#include "Modifier.h"

Flux::Flux()
    : m_amount(0),
      m_changeRate(nullptr),
      m_static(false),
      m_fraction(1.0),
      m_modifier(nullptr),
      m_type("water")
{}
