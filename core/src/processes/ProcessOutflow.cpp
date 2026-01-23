#include "ProcessOutflow.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflow::ProcessOutflow(WaterContainer* container)
    : Process(container) {}

void ProcessOutflow::RegisterProcessParametersAndForcing(SettingsModel*) {
    // No forcing or parameters
}

bool ProcessOutflow::IsValid() const {
    if (_outputs.size() != 1) {
        wxLogError(_("An outflow should have a single output."));
        return false;
    }

    return true;
}

int ProcessOutflow::GetConnectionCount() const {
    return _outputs.size();
}

double* ProcessOutflow::GetValuePointer(const string& name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }

    return nullptr;
}
