#include "ProcessOutflowLinear.h"
#include "Brick.h"

ProcessOutflowLinear::ProcessOutflowLinear(Brick* brick)
    : ProcessOutflow(brick),
      m_responseFactor(nullptr)
{}

bool ProcessOutflowLinear::IsOk() {
    if (m_outputs.size() != 1) {
        wxLogError(_("The linear storage should have a single output."));
        return false;
    }

    return true;
}

void ProcessOutflowLinear::AssignParameters(const ProcessSettings &processSettings) {
    Process::AssignParameters(processSettings);
    m_responseFactor = GetParameterValuePointer(processSettings, "responseFactor");
}

vecDouble ProcessOutflowLinear::GetChangeRates() {
    return {(*m_responseFactor) * m_brick->GetContent()};
}

double* ProcessOutflowLinear::GetValuePointer(const wxString& name) {
    if (name.IsSameAs("output")) {
        return m_outputs[0]->GetAmountPointer();
    }

    return nullptr;
}