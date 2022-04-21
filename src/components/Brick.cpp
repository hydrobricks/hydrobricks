#include "Brick.h"

#include "HydroUnit.h"

Brick::Brick(HydroUnit *hydroUnit)
    : m_contentPrev(0),
      m_contentNext(0),
      m_hydroUnit(hydroUnit)
{
    if (hydroUnit) {
        hydroUnit->AddBrick(this);
    }
}

void Brick::Finalize() {
    m_contentPrev = m_contentNext;
}