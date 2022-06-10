#include "ProcessRunoffSocont.h"
#include "Brick.h"
#include "WaterContainer.h"

ProcessRunoffSocont::ProcessRunoffSocont(WaterContainer* container)
    : ProcessOutflow(container),
      m_slope(nullptr),
      m_runoffParameter(nullptr),
      m_h(0)
{}

void ProcessRunoffSocont::AssignParameters(const ProcessSettings &processSettings) {
    Process::AssignParameters(processSettings);
    m_slope = GetParameterValuePointer(processSettings, "slope");
    m_runoffParameter = GetParameterValuePointer(processSettings, "runoffParameter");
}

vecDouble ProcessRunoffSocont::GetChangeRates() {
    // Unit conversion: [d] to [s]
    double dt = g_timeStepInDays * g_dayInSec;

    // Unit conversion: [mm/d] to [m/s]
    double influx = m_container->GetContentWithChanges() * 0.001 / dt;

    double runoff = *m_runoffParameter * pow(*m_slope, 0.5) * pow(m_h, 5/3); // [m/s]

    double shapeCoefficient = 0.5;
    double dh = (influx - runoff) / shapeCoefficient * dt; // [m]
    m_h += dh;

    return {influx - dh/dt * shapeCoefficient};
}