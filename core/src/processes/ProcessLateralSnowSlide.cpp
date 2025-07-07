#include "Brick.h"
#include "ProcessLateralSnowSlide.h"
#include "WaterContainer.h"

ProcessLateralSnowSlide::ProcessLateralSnowSlide(WaterContainer* container)
    : ProcessLateral(container) {}

bool ProcessLateralSnowSlide::IsOk() {
    if (_outputs.empty()) {
        wxLogError(_("A SnowSlide process must have at least 1 output."));
        return false;
    }

    return true;
}
