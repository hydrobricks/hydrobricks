#include "Brick.h"
#include "ProcessTransformSnowToIceConstant.h"
#include "WaterContainer.h"

ProcessTransformSnowToIceConstant::ProcessTransformSnowToIceConstant(WaterContainer* container)
    : ProcessTransform(container),
      m_rate(nullptr) {}

void ProcessTransformSnowToIceConstant::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("snow_ice_transformation_rate", 0.002f);
}

void ProcessTransformSnowToIceConstant::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    if (HasParameter(processSettings, "snow_ice_transformation_rate")) {
        m_rate = GetParameterValuePointer(processSettings, "snow_ice_transformation_rate");
    } else {
        m_rate = GetParameterValuePointer(processSettings, "rate");
    }
}

vecDouble ProcessTransformSnowToIceConstant::GetRates() {
    return {*m_rate};
}
