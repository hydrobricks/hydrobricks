#include "Storage.h"

Storage::Storage(HydroUnit *hydroUnit)
    : Brick(hydroUnit),
      m_capacity(UNDEFINED)
{}