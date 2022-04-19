#include "StorageLinear.h"

StorageLinear::StorageLinear(HydroUnit *hydroUnit)
    : Storage(hydroUnit),
      m_responseFactor(nullptr)
{}

bool StorageLinear::IsOk() {
    if (!Storage::IsOk()) return false;
    if (m_Outputs.size() != 1) {
        wxLogError(_("The linear storage should have a single output."));
        return false;
    }

    return true;
}

bool StorageLinear::Compute() {

    double Qout = (*m_responseFactor) * m_waterContent;

    return false;
}
