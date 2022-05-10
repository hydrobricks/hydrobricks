#include "TimeSeriesUniform.h"

TimeSeriesUniform::TimeSeriesUniform(VariableType type)
    : TimeSeries(type),
      m_data(nullptr)
{}

TimeSeriesUniform::~TimeSeriesUniform() {
    wxDELETE(m_data);
}

bool TimeSeriesUniform::SetCursorToDate(const wxDateTime &dateTime) {
    wxASSERT(m_data);
    if (!m_data->SetCursorToDate(dateTime)) {
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

wxDateTime TimeSeriesUniform::GetStart() {
    wxASSERT(m_data);
    return m_data->GetStart();
}

wxDateTime TimeSeriesUniform::GetEnd() {
    wxASSERT(m_data);
    return m_data->GetEnd();
}

TimeSeriesData* TimeSeriesUniform::GetDataPointer(int) {
    wxASSERT(m_data);
    return m_data;
}