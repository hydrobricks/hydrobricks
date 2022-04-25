#include "Flux.h"

#include "Modifier.h"

Flux::Flux()
    : m_amountPrev(0),
      m_amountNext(0),
      m_modifier(nullptr)
{}

void Flux::Finalize() {
    m_amountPrev = m_amountNext;
}