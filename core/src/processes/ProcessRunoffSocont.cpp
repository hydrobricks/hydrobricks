#include "ProcessRunoffSocont.h"

#include "Brick.h"
#include "HydroUnit.h"
#include "WaterContainer.h"

ProcessRunoffSocont::ProcessRunoffSocont(WaterContainer* container)
    : ProcessOutflow(container),
      m_slope(0),
      m_beta(nullptr),
      m_areaUnit(0),
      m_areaFraction(nullptr) {}

void ProcessRunoffSocont::SetHydroUnitProperties(HydroUnit* unit, Brick* brick) {
    if (brick->IsLandCover()) {
        auto* landCover = dynamic_cast<LandCover*>(brick);
        m_areaFraction = landCover->GetAreaFractionPointer();
    }
    m_areaUnit = unit->GetArea();
    m_slope = unit->GetPropertyDouble("slope", "m/m");
}

void ProcessRunoffSocont::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    m_beta = GetParameterValuePointer(processSettings, "beta");
}

double ProcessRunoffSocont::GetArea() {
    if (m_areaFraction) {
        return m_areaUnit * *m_areaFraction;
    }

    return m_areaUnit;
}

vecDouble ProcessRunoffSocont::GetRates() {
    // Considers the runoff on an inclined plane with a water depth of 0 at the top and of h at the bottom.
    // The water depth is assumed to be linear from the top to the bottom of the plane.
    // The storage shape is the ratio between the water depth at the bottom and the average water depth -> 2.
    double storageShape = 2.0;
    // h is the water depth at the bottom of the plane != average water depth
    double h = m_container->GetContentWithChanges() * storageShape * 1000;  // [m]
    double qquick = *m_beta * pow(m_slope, 0.5) * pow(h, 5 / 3);  // [m^3/s]
    const double dt = 86400;  // [s] number of seconds in a day
    // Computation in two steps:
    // double dh = (qquick / GetArea()) * dt * m_storageShape; // [m/d] dh at the plane bottom
    // double runoff = 1000 * dh / m_storageShape ; // [mm/d]
    // Simplified computation:
    double runoff = 1000 * (qquick / GetArea()) * dt;  // [mm/d]

    return {wxMin(runoff, m_container->GetContentWithChanges())};
}
