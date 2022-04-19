#include "FluxDirect.h"

FluxDirect::FluxDirect()
{}

bool FluxDirect::IsOk() {
    if (m_in == nullptr) {
        wxLogError(_("The in flux is not connected to a brick."));
        return false;
    }
    if (m_out == nullptr) {
        wxLogError(_("The out flux is not connected to a brick."));
        return false;
    }

    return true;
}

double FluxDirect::GetAmount() {
    return m_amount;
}