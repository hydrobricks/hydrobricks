#include "StorageLinear.h"

StorageLinear::StorageLinear(HydroUnit *hydroUnit)
    : Storage(hydroUnit),
      m_responseFactor(0)
{}

double StorageLinear::GetOutput() {
    return (*m_responseFactor) * m_waterContent;
}