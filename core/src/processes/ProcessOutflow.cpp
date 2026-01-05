#include "ProcessOutflow.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflow::ProcessOutflow(WaterContainer* container)
    : Process(container) {}

void ProcessOutflow::RegisterProcessParametersAndForcing(SettingsModel*) {
    // Nothing to register
}

bool ProcessOutflow::IsOk() {
    if (_outputs.size() != 1) {
        wxLogError(_("An outflow should have a single output."));
        return false;
    }

    return true;
}

int ProcessOutflow::GetConnectionCount() {
    return 1;
}

double* ProcessOutflow::GetValuePointer(const string& name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }

    return nullptr;
}
