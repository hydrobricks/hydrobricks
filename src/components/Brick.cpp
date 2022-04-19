#include "Brick.h"

#include "HydroUnit.h"

Brick::Brick(HydroUnit *hydroUnit)
    : m_waterContent(0),
      m_hydroUnit(hydroUnit)
{
    if (hydroUnit) {
        hydroUnit->AddBrick(this);
    }
}
