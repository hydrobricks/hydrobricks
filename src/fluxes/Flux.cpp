#include "Flux.h"

#include "Modifier.h"

Flux::Flux()
    : m_amount(0),
      m_changeRate(nullptr),
      m_static(false),
      m_modifier(nullptr)
{}
