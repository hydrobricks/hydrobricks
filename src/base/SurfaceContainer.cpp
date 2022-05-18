#include "SurfaceContainer.h"

SurfaceContainer::SurfaceContainer(HydroUnit *hydroUnit)
    : m_hydroUnit(hydroUnit),
      m_snowRainSplitter(nullptr)
{}

SurfaceContainer::~SurfaceContainer() {
    wxDELETE(m_snowRainSplitter);
}
