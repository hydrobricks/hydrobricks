#include "ProcessMelt.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessMelt::ProcessMelt(WaterContainer* container)
    : Process(container) {}

bool ProcessMelt::IsOk() {
    if (_outputs.size() != 1) {
        wxLogError(_("Melt processes should have a single output."));
        return false;
    }

    return true;
}

int ProcessMelt::GetConnectionCount() {
    return 1;
}

double* ProcessMelt::GetValuePointer(const string& name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }

    return nullptr;
}
