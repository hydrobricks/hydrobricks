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
    if (m_Inputs.empty()) {
        wxLogError(_("The storage is not attached to inputs."));
        return false;
    }
    if (m_Outputs.empty()) {
        wxLogError(_("The storage is not attached to outputs."));
        return false;
    }

    return true;
}

bool Storage::Compute() {
    return false;
}

double Storage::SumIncomingFluxes() {
    wxASSERT(!m_Inputs.empty());

    if (m_Inputs.size() == 1) {
        return m_Inputs[0]->GetAmount();
    } else {
        double sum = 0;
        for (auto & input : m_Inputs) {
            sum += input->GetAmount();
        }
        return sum;
    }
}