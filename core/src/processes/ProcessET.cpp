#include "ProcessET.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessET::ProcessET(WaterContainer* container)
    : Process(container) {}

bool ProcessET::IsOk() {
    if (_outputs.size() != 1) {
        wxLogError(_("ET should have a single output."));
        return false;
    }

    return true;
}

int ProcessET::GetConnectionsNb() {
    return 1;
}

double* ProcessET::GetValuePointer(const string& name) {
    if (name == "output") {
        return _outputs[0]->GetAmountPointer();
    }

    return nullptr;
}
