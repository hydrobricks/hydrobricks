#include "Forcing.h"

Forcing::Forcing(VariableType type)
    : m_type(type),
      m_timeSeriesData(nullptr)
{}

void Forcing::AttachTimeSeriesData(TimeSeriesData* timeSeriesData) {
    wxASSERT(timeSeriesData);
    m_timeSeriesData = timeSeriesData;
}

double Forcing::GetValue() {
    return m_timeSeriesData->GetCurrentValue();
}
