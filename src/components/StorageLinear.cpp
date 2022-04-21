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
    double qOut = (*m_responseFactor) * m_contentPrev;
    double dS = qIn - qOut;
    m_contentPrev += dS;
    m_outputs[0]->SetAmountPrev(qOut);

    return true;
}
