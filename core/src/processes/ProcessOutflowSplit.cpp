#include "ProcessOutflowSplit.h"

#include <algorithm>

#include "Brick.h"
#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessOutflowSplit::ProcessOutflowSplit(WaterContainer* container)
    : ProcessOutflow(container),
      _splitFraction(nullptr) {}

void ProcessOutflowSplit::RegisterProcessSettings(SettingsModel* modelSettings) {
    // Default: the PREVAH SLOWCOMP recharge split (8/9 vs 1/9).
    modelSettings->AddProcessParameter("split_fraction", 0.8889f);
}

bool ProcessOutflowSplit::IsValid() const {
    if (_outputs.size() != 2) {
        LogError("A split outflow must have exactly two outputs.");
        return false;
    }
    if (_splitFraction == nullptr) {
        LogError("Split outflow process: missing the 'split_fraction' parameter.");
        return false;
    }
    if (*_splitFraction < 0 || *_splitFraction > 1) {
        LogError("Split outflow process: the 'split_fraction' parameter must be in [0, 1].");
        return false;
    }

    // The split drains the entire container content. Any sibling process drawing from the same
    // container would compete for the same water and distort the split ratio through the
    // container's non-negativity constraint.
    Brick* brick = _container->GetParentBrick();
    if (brick != nullptr) {
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            Process* process = brick->GetProcess(i);
            if (process == this) {
                continue;
            }
            if (process->GetWaterContainer() == _container) {
                LogError(
                    "A split outflow ('{}') takes the entire content of the container; it cannot run in parallel "
                    "with another process ('{}') drawing from the same storage.",
                    GetName(), process->GetName());
                return false;
            }
        }
    }

    return true;
}

void ProcessOutflowSplit::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _splitFraction = GetParameterValuePointer(processSettings, "split_fraction");
}

const vecDouble& ProcessOutflowSplit::GetRates() {
    double content = std::max(_container->GetContentWithChanges(), 0.0);

    // GetRates() must return rates (amount per unit time): dividing the content by the time step
    // drains exactly the content over one step (as in ProcessOutflowSnowHolding). The 1.0 fallback
    // covers the case where no time machine is wired up (e.g. unit tests).
    double timeStep = (_timeMachine != nullptr) ? *_timeMachine->GetTimeStepPointer() : 1.0;

    double fraction = static_cast<double>(*_splitFraction);

    return StoreRates({fraction * content / timeStep, (1.0 - fraction) * content / timeStep});
}
