#include "ProcessTransform.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessTransform::ProcessTransform(WaterContainer* container)
    : Process(container) {}

void ProcessTransform::RegisterProcessParametersAndForcing(SettingsModel*) {
    // Nothing to register
}

bool ProcessTransform::IsOk() {
    if (m_outputs.size() != 1) {
        wxLogError(_("A transform process should have a single output."));
        return false;
    }

    return true;
}

int ProcessTransform::GetConnectionsNb() {
    return 1;
}

double* ProcessTransform::GetValuePointer(const string& name) {
    if (name == "output") {
        return m_outputs[0]->GetAmountPointer();
    }

    return nullptr;
}
