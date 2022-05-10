#include "ProcessOutflow.h"
#include "Brick.h"

ProcessOutflow::ProcessOutflow(Brick* brick)
    : Process(brick)
{}

bool ProcessOutflow::IsOk() {
    if (m_outputs.size() != 1) {
        wxLogError(_("The linear storage should have a single output."));
        return false;
    }

    return true;
}

double* ProcessOutflow::GetValuePointer(const wxString& name) {
    if (name.IsSameAs("output")) {
        return m_outputs[0]->GetAmountPointer();
    }

    return nullptr;
}