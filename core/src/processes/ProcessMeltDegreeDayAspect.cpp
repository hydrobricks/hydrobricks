#include "ProcessMeltDegreeDayAspect.h"

#include "Brick.h"
#include "HydroUnit.h"
#include "WaterContainer.h"

ProcessMeltDegreeDayAspect::ProcessMeltDegreeDayAspect(WaterContainer* container)
    : ProcessMelt(container),
      m_temperature(nullptr),
      m_degreeDayFactor(nullptr),
      m_meltingTemperature(nullptr) {}

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
    m_aspect_class = unit->GetPropertyString("aspect_class");
}

void ProcessMeltDegreeDayAspect::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    m_meltingTemperature = GetParameterValuePointer(processSettings, "melting_temperature");

    if (m_aspect_class == "north" || m_aspect_class == "NE") {
        m_degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor_n");
    } else if (m_aspect_class == "south" || m_aspect_class == "S") {
        m_degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor_s");
    } else if (m_aspect_class == "east" || m_aspect_class == "west" || m_aspect_class == "E" || m_aspect_class == "W") {
        m_degreeDayFactor = GetParameterValuePointer(processSettings, "degree_day_factor_ew");
    } else {
        throw InvalidArgument("Invalid aspect: " + m_aspect_class);
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
