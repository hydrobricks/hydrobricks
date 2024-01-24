#include "ProcessMeltDegreeDayAspect.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessMeltDegreeDayAspect::ProcessMeltDegreeDayAspect(WaterContainer* container)
    : ProcessMelt(container),
      m_temperature(nullptr),
      m_degreeDayFactorN(nullptr),
      m_degreeDayFactorS(nullptr),
      m_degreeDayFactorEW(nullptr),
      m_meltingTemperature(nullptr) {}

bool ProcessMeltDegreeDayAspect::IsOk() {
    if (!ProcessMelt::IsOk()) {
        return false;
    }
    if (m_temperature == nullptr) {
        return false;
    }
    if (m_degreeDayFactorN == nullptr) {
        return false;
    }
    if (m_degreeDayFactorS == nullptr) {
            return false;
    }
    if (m_degreeDayFactorEW == nullptr) {
            return false;
    }
    if (m_meltingTemperature == nullptr) {
        return false;
    }

    return true;
}

void ProcessMeltDegreeDayAspect::SetHydroUnitProperties(HydroUnit* unit, Brick* brick) {
    m_aspect = unit->GetPropertyString("aspect");
}

void ProcessMeltDegreeDayAspect::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    m_degreeDayFactorN = GetParameterValuePointer(processSettings, "degree_day_factor_n");
    m_degreeDayFactorS = GetParameterValuePointer(processSettings, "degree_day_factor_s");
    m_degreeDayFactorEW = GetParameterValuePointer(processSettings, "degree_day_factor_ew");
    m_meltingTemperature = GetParameterValuePointer(processSettings, "melting_temperature");
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
