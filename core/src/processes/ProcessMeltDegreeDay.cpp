#include "ProcessMeltDegreeDay.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessMeltDegreeDay::ProcessMeltDegreeDay(WaterContainer* container)
    : ProcessMelt(container),
      m_temperature(nullptr),
      m_degreeDayFactor(nullptr),
      m_meltingTemperature(nullptr) {}

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

void ProcessMeltDegreeDay::AssignParameters(const ProcessSettings& processSettings) {
    Process::AssignParameters(processSettings);
    m_degreeDayFactor = GetParameterValuePointer(processSettings, "degreeDayFactor");
    m_meltingTemperature = GetParameterValuePointer(processSettings, "meltingTemperature");
}

void ProcessMeltDegreeDay::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == Temperature) {
        m_temperature = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type Temperature");
    }
}

vecDouble ProcessMeltDegreeDay::GetRates() {
    double melt = 0;
    if (m_temperature->GetValue() >= *m_meltingTemperature) {
        melt = (m_temperature->GetValue() - *m_meltingTemperature) * *m_degreeDayFactor;
    }

    return {melt};
}
