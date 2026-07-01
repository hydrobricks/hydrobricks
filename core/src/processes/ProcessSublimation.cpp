#include "ProcessSublimation.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessSublimation::ProcessSublimation(WaterContainer* container)
    : Process(container) {}

bool ProcessSublimation::IsValid() const {
    if (_outputs.size() != 1) {
        LogError("Sublimation should have a single output.");
        return false;
    }

    return true;
}

int ProcessSublimation::GetConnectionCount() const {
    return 1;
}

double* ProcessSublimation::GetValuePointer(std::string_view name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }

    return nullptr;
}
