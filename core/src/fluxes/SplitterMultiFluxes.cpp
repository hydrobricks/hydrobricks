#include "SplitterMultiFluxes.h"

SplitterMultiFluxes::SplitterMultiFluxes()
    : Splitter() {}

bool SplitterMultiFluxes::IsOk() {
    if (m_outputs.empty()) {
        wxLogError(_("SplitterMultiFluxes has no input."));
        return false;
    }

    return true;
}

void SplitterMultiFluxes::SetParameters(const SplitterSettings&) {
    // No parameter
}

double* SplitterMultiFluxes::GetValuePointer(const string& name) {
    if (name == "output_1") {
        return m_outputs[0]->GetAmountPointer();
    } else if (name == "output_2") {
        return m_outputs[1]->GetAmountPointer();
    } else if (name == "output_3") {
        return m_outputs[2]->GetAmountPointer();
    } else if (name == "output_4") {
        return m_outputs[3]->GetAmountPointer();
    } else if (name == "output_5") {
        return m_outputs[4]->GetAmountPointer();
    }

    throw ConceptionIssue(_("Output cannot be find."));
}

void SplitterMultiFluxes::Compute() {
    wxASSERT(!m_inputs.empty());
    for (auto output : m_outputs) {
        output->UpdateFlux(m_inputs[0]->GetAmount());
    }
}
