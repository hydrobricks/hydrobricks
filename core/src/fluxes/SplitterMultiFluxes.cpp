#include "SplitterMultiFluxes.h"

SplitterMultiFluxes::SplitterMultiFluxes()
    : Splitter() {}

bool SplitterMultiFluxes::IsOk() {
    if (_outputs.empty()) {
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
        return _outputs[0]->GetAmountPointer();
    } else if (name == "output_2") {
        return _outputs[1]->GetAmountPointer();
    } else if (name == "output_3") {
        return _outputs[2]->GetAmountPointer();
    } else if (name == "output_4") {
        return _outputs[3]->GetAmountPointer();
    } else if (name == "output_5") {
        return _outputs[4]->GetAmountPointer();
    }

    throw ConceptionIssue(_("Output cannot be find."));
}

void SplitterMultiFluxes::Compute() {
    wxASSERT(!_inputs.empty());
    for (auto output : _outputs) {
        output->UpdateFlux(_inputs[0]->GetAmount());
    }
}
