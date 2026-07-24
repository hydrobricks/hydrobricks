#include "ProcessOutflowRest.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessOutflowRest::ProcessOutflowRest(WaterContainer* container)
    : ProcessOutflow(container) {}

void ProcessOutflowRest::RegisterProcessSettings(SettingsModel*) {
    // Nothing to register
}

const vecDouble& ProcessOutflowRest::GetRates() {
    double rest = _container->GetContentWithChanges();

    Brick* brick = _container->GetParentBrick();
    if (brick != nullptr && brick->NeedsSolver()) {
        // Solver brick: the other processes have not been applied yet, so the content is still full.
        // Subtract their CURRENT outflow rates (linked change-rate pointers, not the stored amounts)
        // so that the demands sum to the available content and the split is not distorted by the
        // container's non-negativity constraint. Note: under multi-stage solvers (Heun, RK4) the
        // sibling rate seen in corrector stages is one stage stale, since the change-rate pointers
        // are only (re)linked in the constrained pass; the result stays stable and mass-conserving
        // (exact on Euler). The mm-vs-rate subtraction follows the dt = 1 convention of this process.
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            Process* process = brick->GetProcess(i);
            if (process == this) {
                continue;
            }
            for (int j = 0; j < process->GetOutputFluxCount(); ++j) {
                double* rate = process->GetOutputFlux(j)->GetChangeRatePointer();
                if (rate != nullptr) {
                    rest -= *rate;
                }
            }
        }
    }
    // Direct brick: the content already reflects the siblings' withdrawals, so take it as-is.

    return StoreRates({std::max(rest, 0.0)});
}
