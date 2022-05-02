#include "StorageLinear.h"

StorageLinear::StorageLinear(HydroUnit *hydroUnit)
    : Storage(hydroUnit),
      m_responseFactor(nullptr)
{
    m_needsSolver = true;
}

void StorageLinear::AssignParameters(const BrickSettings &brickSettings) {
    Storage::AssignParameters(brickSettings);
    m_responseFactor = GetParameterValuePointer(brickSettings, "responseFactor");
}

bool StorageLinear::IsOk() {
    if (!Storage::IsOk()) return false;
    if (m_outputs.size() != 1) {
        wxLogError(_("The linear storage should have a single output."));
        return false;
    }

    return true;
}

vecDouble StorageLinear::ComputeOutputs() {
    double qOut = (*m_responseFactor) * m_content;

    return {qOut};
}

double* StorageLinear::GetValuePointer(const wxString& name) {
    if (name.IsSameAs("output")) {
        return m_outputs[0]->GetAmountPointer();
    }

    return nullptr;
}