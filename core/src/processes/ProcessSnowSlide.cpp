#include "ProcessSnowSlide.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessSnowSlide::ProcessSnowSlide(WaterContainer* container)
    : Process(container) {}

bool ProcessSnowSlide::IsOk() {
    if (m_outputs.empty()) {
        wxLogError(_("A SnowSlide process must have at least 1 output."));
        return false;
    }

    return true;
}
