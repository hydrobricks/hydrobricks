#include "ProcessMeltTemperatureIndex.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessMeltTemperatureIndex::ProcessMeltTemperatureIndex(WaterContainer* container)
    : ProcessMelt(container),
      m_temperature(nullptr),
      m_potentialClearSkyDirectSolarRadiation(nullptr),
      m_meltFactor(nullptr),
      m_meltingTemperature(nullptr),
      m_radiationCoefficient(nullptr) {}

void ProcessMeltTemperatureIndex::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("melt_factor", 3.0f);
    modelSettings->AddProcessParameter("melting_temperature", 0.0f);
    modelSettings->AddProcessParameter("radiation_coefficient", 0.0007f);
    modelSettings->AddProcessForcing("temperature");
    modelSettings->AddProcessForcing("solar_radiation");
}

bool ProcessMeltTemperatureIndex::IsOk() {
    if (!ProcessMelt::IsOk()) {
        return false;
    }
    if (m_temperature == nullptr) {
        return false;
    }
    if (m_meltFactor == nullptr) {
        return false;
    }
    if (m_meltingTemperature == nullptr) {
        return false;
    }
    if (m_radiationCoefficient == nullptr) {
        return false;
    }
    if (m_potentialClearSkyDirectSolarRadiation == nullptr) {
        return false;
    }

    return true;
}

void ProcessMeltTemperatureIndex::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    m_meltFactor = GetParameterValuePointer(processSettings, "melt_factor");
    m_meltingTemperature = GetParameterValuePointer(processSettings, "melting_temperature");
    m_radiationCoefficient = GetParameterValuePointer(processSettings, "radiation_coefficient");
}

void ProcessMeltTemperatureIndex::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == Temperature) {
        m_temperature = forcing;
    } else if (forcing->GetType() == Radiation) {
        m_potentialClearSkyDirectSolarRadiation = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type Temperature or Radiation");
    }
}

vecDouble ProcessMeltTemperatureIndex::GetRates() {
    if (!m_container->ContentAccessible()) {
        return {0};
    }

    double melt = 0;
    if (m_temperature->GetValue() >= *m_meltingTemperature) {
        melt = (m_temperature->GetValue() - *m_meltingTemperature) *
               (*m_meltFactor + *m_radiationCoefficient * m_potentialClearSkyDirectSolarRadiation->GetValue());
    }

    return {melt};
}
