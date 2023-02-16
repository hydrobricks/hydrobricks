#include "Behaviour.h"

#include "ModelHydro.h"

Behaviour::Behaviour()
    : m_manager(nullptr),
      m_cursor(0) {}

bool Behaviour::Apply(double) {
    return false;
}