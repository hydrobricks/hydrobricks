
#include "StorageLinear.h"

StorageLinear::StorageLinear(HydroUnit *hydroUnit)
    : Storage(hydroUnit)
{}

double StorageLinear::GetOutput() {
    return m_responseFactor * m_waterContent;
}