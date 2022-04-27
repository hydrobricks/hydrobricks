#include "StorageLinear.h"

StorageLinear::StorageLinear(HydroUnit *hydroUnit)
    : Storage(hydroUnit),
      m_responseFactor(nullptr)
{
    m_needsSolver = true;
}

bool StorageLinear::IsOk() {
    if (!Storage::IsOk()) return false;
    if (m_outputs.size() != 1) {
        wxLogError(_("The linear storage should have a single output."));
        return false;
    }

    return true;
}

std::vector<double> StorageLinear::ComputeOutputs() {
    double qOut = (*m_responseFactor) * m_content;

    return {qOut};
}
