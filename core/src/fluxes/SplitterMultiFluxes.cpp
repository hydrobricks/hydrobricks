#include "SplitterMultiFluxes.h"

SplitterMultiFluxes::SplitterMultiFluxes()
    : Splitter()
{}

bool SplitterMultiFluxes::IsOk() {
    if (m_outputs.size() < 2) {
        wxLogError(_("SplitterMultiFluxes should have at least 2 outputs."));
        return false;
    }
    if (m_outputs.empty()) {
        wxLogError(_("SplitterMultiFluxes has no input."));
        return false;
    }

    return true;
}

void SplitterMultiFluxes::AssignParameters(const SplitterSettings &) {
    // No parameter
}

double* SplitterMultiFluxes::GetValuePointer(const std::string& name) {
    if (name == "output-1") {
        return m_outputs[0]->GetAmountPointer();
    } else if (name == "output-2") {
        return m_outputs[1]->GetAmountPointer();
    } else if (name == "output-3") {
        return m_outputs[2]->GetAmountPointer();
    } else if (name == "output-4") {
        return m_outputs[3]->GetAmountPointer();
    } else if (name == "output-5") {
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