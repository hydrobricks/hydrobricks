#include "TimeSeriesUniform.h"

TimeSeriesUniform::TimeSeriesUniform(VariableType type)
    : TimeSeries(type),
      m_data(nullptr)
{}

TimeSeriesUniform::~TimeSeriesUniform() {
    wxDELETE(m_data);
}

bool TimeSeriesUniform::SetCursorToDate(double date) {
    wxASSERT(m_data);
    if (!m_data->SetCursorToDate(date)) {
        return false;
    }

    return true;
}

bool TimeSeriesUniform::AdvanceOneTimeStep() {
    wxASSERT(m_data);
    if (!m_data->AdvanceOneTimeStep()) {
        return false;
    }

    return true;
}

double TimeSeriesUniform::GetStart() {
    wxASSERT(m_data);
    return m_data->GetStart();
}

double TimeSeriesUniform::GetEnd() {
    wxASSERT(m_data);
    return m_data->GetEnd();
}

TimeSeriesData* TimeSeriesUniform::GetDataPointer(int) {
    wxASSERT(m_data);
    return m_data;
}