#include "Flux.h"

#include "Modifier.h"

Flux::Flux()
    : m_isConstant(false),
      m_amount(0),
      m_amountNext(0),
      m_modifier(nullptr)
{}

void Flux::Finalize() {
    m_amount = m_amountNext;
}