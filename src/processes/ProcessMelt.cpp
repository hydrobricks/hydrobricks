#include "Brick.h"
#include "ProcessMelt.h"

ProcessMelt::ProcessMelt(Brick* brick)
    : Process(brick)
{}

bool ProcessMelt::IsOk() {
    if (m_outputs.size() != 1) {
        wxLogError(_(" Melt processes should have a single output."));
        return false;
    }

    return true;
}

int ProcessMelt::GetConnectionsNb() {
    return 1;
}

double* ProcessMelt::GetValuePointer(const wxString& name) {
    if (name.IsSameAs("output")) {
        return m_outputs[0]->GetAmountPointer();
    }

    return nullptr;
}