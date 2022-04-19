#include "StorageLinear.h"

StorageLinear::StorageLinear(HydroUnit *hydroUnit)
    : Storage(hydroUnit),
      m_responseFactor(nullptr)
{}

bool StorageLinear::Compute() {

    double Qout = (*m_responseFactor) * m_waterContent;

    return false;
}
