#include "ProcessRunoffSocont.h"

#include "Brick.h"
#include "HydroUnit.h"
#include "WaterContainer.h"

ProcessRunoffSocont::ProcessRunoffSocont(WaterContainer* container)
    : ProcessOutflow(container),
      m_slope(0),
      m_beta(nullptr),
      m_areaFraction(nullptr),
      m_areaUnit(0),
      m_exponent(5.0 / 3.0) {}

void ProcessRunoffSocont::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("beta", 500.0f);
}

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
    const double dt = 86400;  // [s] number of seconds in a day

    // h is the water depth at the bottom of the plane != average water depth
    double h = m_container->GetContentWithChanges() * storageShape / 1000;  // [m]

    double qQuick = *m_beta * pow(m_slope, 0.5) * pow(h, m_exponent) / GetArea();  // [m/s]
    double dh = qQuick * storageShape * dt;                                        // [m]
    double runoff = (dh / storageShape) * 1000;                                    // [mm]

    return {wxMin(runoff, m_container->GetContentWithChanges())};
}
