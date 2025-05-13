#include "ProcessTransformSnowToIce.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessTransformSnowToIce::ProcessTransformSnowToIce(WaterContainer* container)
    : ProcessTransform(container),
      m_rate(nullptr) {}

void ProcessTransformSnowToIce::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("snow_ice_transformation_rate", 0.002f);
}

void ProcessTransformSnowToIce::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    if (HasParameter(processSettings, "snow_ice_transformation_rate")) {
        m_rate = GetParameterValuePointer(processSettings, "snow_ice_transformation_rate");
    } else {
        m_rate = GetParameterValuePointer(processSettings, "rate");
    }
}

vecDouble ProcessTransformSnowToIce::GetRates() {
    return {*m_rate};
}
