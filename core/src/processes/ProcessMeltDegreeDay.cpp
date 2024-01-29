#include "ProcessMeltDegreeDay.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessMeltDegreeDay::ProcessMeltDegreeDay(WaterContainer* container)
    : ProcessMelt(container),
      m_temperature(nullptr),
      m_degreeDayFactor(nullptr),
      m_meltingTemperature(nullptr) {}

void ProcessMeltDegreeDay::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("degree_day_factor", 3.0f);
    modelSettings->AddProcessParameter("melting_temperature", 0.0f);
    modelSettings->AddProcessForcing("temperature");
}

bool ProcessMeltDegreeDay::IsOk() {
    if (!ProcessMelt::IsOk()) {
        return false;
    }
    if (m_temperature == nullptr) {
        return false;
    }
    if (m_degreeDayFactor == nullptr) {
        return false;
    }
    if (m_meltingTemperature == nullptr) {
        return false;
    }

    return true;
}

void ProcessMeltDegreeDay::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    m_degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor");
    m_meltingTemperature = GetParameterValuePointer(processSettings, "melting_temperature");
}

void ProcessMeltDegreeDay::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == Temperature) {
        m_temperature = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type Temperature");
    }
}

vecDouble ProcessMeltDegreeDay::GetRates() {
    if (!m_container->ContentAccessible()) {
        return {0};
    }

    double melt = 0;
    if (m_temperature->GetValue() >= *m_meltingTemperature) {
        melt = (m_temperature->GetValue() - *m_meltingTemperature) * *m_degreeDayFactor;
    }

    return {melt};
}
