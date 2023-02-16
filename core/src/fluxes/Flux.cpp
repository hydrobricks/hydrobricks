#include "Flux.h"

#include "Modifier.h"

Flux::Flux()
    : m_amount(0),
      m_changeRate(nullptr),
      m_static(false),
      m_needsWeighting(false),
      m_fractionUnitArea(1.0),
      m_fractionLandCover(1.0),
      m_fractionTotal(1.0),
      m_modifier(nullptr),
      m_type("water") {}

void Flux::Reset() {
    m_amount = 0;
}

void Flux::UpdateFlux(double amount) {
    if (m_fractionTotal < 1.0) {
        m_amount = amount * m_fractionTotal;
    } else {
        m_amount = amount;
    }
}
