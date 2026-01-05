#include "ProcessInfiltration.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessInfiltration::ProcessInfiltration(WaterContainer* container)
    : Process(container),
      _targetBrick(nullptr) {}

bool ProcessInfiltration::IsOk() {
    if (_outputs.size() != 1) {
        wxLogError(_("An infiltration process should have a single output."));
        return false;
    }
    if (_targetBrick == nullptr) {
        wxLogError(_("An infiltration process must be linked to a target brick."));
        return false;
    }

    return true;
}

int ProcessInfiltration::GetConnectionCount() {
    return 1;
}

double* ProcessInfiltration::GetValuePointer(const string& name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }

    return nullptr;
}

double ProcessInfiltration::GetTargetStock() {
    return _targetBrick->GetWaterContainer()->GetContentWithChanges();
}

double ProcessInfiltration::GetTargetCapacity() {
    return _targetBrick->GetWaterContainer()->GetMaximumCapacity();
}

double ProcessInfiltration::GetTargetFillingRatio() {
    return _targetBrick->GetWaterContainer()->GetTargetFillingRatio();
}
