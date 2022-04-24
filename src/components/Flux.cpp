#include "Flux.h"

#include "Modifier.h"

Flux::Flux()
    : m_in(nullptr),
      m_out(nullptr),
      m_amountPrev(0),
      m_amountNext(0),
      m_modifier(nullptr)
{}

void Flux::Finalize() {
    m_amountPrev = m_amountNext;
}