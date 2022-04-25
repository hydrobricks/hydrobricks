#include "Storage.h"

Storage::Storage(HydroUnit *hydroUnit)
    : Brick(hydroUnit),
      m_capacity(UNDEFINED)
{}

bool Storage::IsOk() {
    if (m_hydroUnit == nullptr) {
        wxLogError(_("The storage is not attached to a hydro unit."));
        return false;
    }
    if (m_inputs.empty()) {
        wxLogError(_("The storage is not attached to inputs."));
        return false;
    }
    if (m_outputs.empty()) {
        wxLogError(_("The storage is not attached to outputs."));
        return false;
    }

    return true;
}

bool Storage::NeedsSolver() {
    return true;
}

bool Storage::Compute() {
    return false;
}

double Storage::SumIncomingFluxes() {
    wxASSERT(!m_inputs.empty());

    if (m_inputs.size() == 1) {
        return m_inputs[0]->GetAmountPrev();
    } else {
        double sum = 0;
        for (auto & input : m_inputs) {
            sum += input->GetAmountPrev();
        }
        return sum;
    }
}