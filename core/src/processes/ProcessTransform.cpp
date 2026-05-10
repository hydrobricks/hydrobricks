#include "ProcessTransform.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessTransform::ProcessTransform(WaterContainer* container)
    : Process(container) {}

void ProcessTransform::RegisterProcessParametersAndForcing(SettingsModel*) {
    // No forcing or parameters
}

bool ProcessTransform::IsValid() const {
    if (_outputs.size() != 1) {
        LogError("A transform process should have a single output.");
        return false;
    }

    return true;
}

int ProcessTransform::GetConnectionCount() const {
    return 1;
}

double* ProcessTransform::GetValuePointer(std::string_view name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }

    return nullptr;
}
