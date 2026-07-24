#include "ProcessOutflow.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflow::ProcessOutflow(WaterContainer* container)
    : Process(container) {}

void ProcessOutflow::RegisterProcessSettings(SettingsModel*) {
    // No forcing or parameters
}

bool ProcessOutflow::IsValid() const {
    if (_outputs.size() != 1) {
        LogError("An outflow should have a single output.");
        return false;
    }

    return true;
}

int ProcessOutflow::GetConnectionCount() const {
    return static_cast<int>(_outputs.size());
}

double* ProcessOutflow::GetValuePointer(std::string_view name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }

    return nullptr;
}
