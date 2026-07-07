#include "ProcessOutflowDirect.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowDirect::ProcessOutflowDirect(WaterContainer* container)
    : ProcessOutflow(container) {}

void ProcessOutflowDirect::RegisterProcessSettings(SettingsModel*) {
    // Nothing to register
}

bool ProcessOutflowDirect::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }

    // A direct outflow takes the entire container content. Any sibling process drawing from the same
    // container would compete for the same water, so the demands would exceed the available content
    // and the split would be distorted by the container's non-negativity constraint.
    Brick* brick = _container->GetParentBrick();
    if (brick != nullptr) {
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            Process* process = brick->GetProcess(i);
            if (process == this) {
                continue;
            }
            if (process->GetWaterContainer() == _container) {
                LogError(
                    "A direct outflow ('{}') takes the entire content of the container; it cannot run in parallel "
                    "with another process ('{}') drawing from the same storage. Use 'outflow:rest' if several "
                    "outflows must share the container.",
                    GetName(), process->GetName());
                return false;
            }
        }
    }

    return true;
}

const vecDouble& ProcessOutflowDirect::GetRates() {
    return StoreRates({std::max(_container->GetContentWithChanges(), 0.0)});
}
