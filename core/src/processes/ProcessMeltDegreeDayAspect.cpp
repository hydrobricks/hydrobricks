#include "ProcessMeltDegreeDayAspect.h"

#include "Brick.h"
#include "HydroUnit.h"
#include "WaterContainer.h"

ProcessMeltDegreeDayAspect::ProcessMeltDegreeDayAspect(WaterContainer* container)
    : ProcessMelt(container),
      m_temperature(nullptr),
      m_degreeDayFactor(nullptr),
      m_meltingTemperature(nullptr) {}

void ProcessMeltDegreeDayAspect::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("degree_day_factor_n", 3);
    modelSettings->AddProcessParameter("degree_day_factor_s", 3);
    modelSettings->AddProcessParameter("degree_day_factor_ew", 3);
    modelSettings->AddProcessParameter("melting_temperature", 0);
    modelSettings->AddProcessForcing("temperature");
}

bool ProcessMeltDegreeDayAspect::IsOk() {
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

void ProcessMeltDegreeDayAspect::SetHydroUnitProperties(HydroUnit* unit, Brick*) {
    m_aspectClass = unit->GetPropertyString("aspect_class");
}

void ProcessMeltDegreeDayAspect::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    m_meltingTemperature = GetParameterValuePointer(processSettings, "melting_temperature");

    if (m_aspectClass == "north" || m_aspectClass == "N") {
        m_degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor_n");
    } else if (m_aspectClass == "south" || m_aspectClass == "S") {
        m_degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor_s");
    } else if (m_aspectClass == "east" || m_aspectClass == "west" || m_aspectClass == "E" || m_aspectClass == "W") {
        if (HasParameter(processSettings, "degree_day_factor_ew")) {
            m_degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor_ew");
        } else if (HasParameter(processSettings, "degree_day_factor_we")) {
            m_degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor_we");
        } else {
            throw InvalidArgument("Missing parameter 'degree_day_factor_ew' or 'degree_day_factor_we'");
        }
    } else {
        throw InvalidArgument("Invalid aspect: " + m_aspectClass);
    }
}

void ProcessMeltDegreeDayAspect::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == Temperature) {
        m_temperature = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type Temperature");
    }
}

vecDouble ProcessMeltDegreeDayAspect::GetRates() {
    if (!m_container->ContentAccessible()) {
        return {0};
    }

    double melt = 0;
    if (m_temperature->GetValue() >= *m_meltingTemperature) {
        melt = (m_temperature->GetValue() - *m_meltingTemperature) * *m_degreeDayFactor;
    }

    return {melt};
}
