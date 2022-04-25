#include "FluxDirect.h"

FluxDirect::FluxDirect()
{}

bool FluxDirect::IsOk() {
    return true;
}

double FluxDirect::GetOutgoingAmount() {
    return m_amountPrev;
}