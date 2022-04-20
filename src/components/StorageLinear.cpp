#include "StorageLinear.h"

StorageLinear::StorageLinear(HydroUnit *hydroUnit)
    : Storage(hydroUnit),
      m_responseFactor(nullptr)
{}

bool StorageLinear::IsOk() {
    if (!Storage::IsOk()) return false;
    if (m_outputs.size() != 1) {
        wxLogError(_("The linear storage should have a single output."));
        return false;
    }

    return true;
}

bool StorageLinear::Compute() {
    double qIn = SumIncomingFluxes();
    double qOut = (*m_responseFactor) * m_waterContent;
    double dS = qIn - qOut;
    m_waterContent += dS;
    m_outputs[0]->SetAmount(qOut);

    return true;
}
