#include "TimeSeriesDistributed.h"

TimeSeriesDistributed::TimeSeriesDistributed(VariableType type)
    : TimeSeries(type)
{}

TimeSeriesDistributed::~TimeSeriesDistributed() {
    for (auto data: m_data) {
        wxDELETE(data);
    }
}

void TimeSeriesDistributed::AddData(TimeSeriesData* data, int unitId) {
    wxASSERT(data);
    m_data.push_back(data);
    m_unitIds.push_back(unitId);
}

bool TimeSeriesDistributed::SetCursorToDate(const wxDateTime &dateTime) {
    for (auto data: m_data) {
        if (!data->SetCursorToDate(dateTime)) {
            return false;
        }
    }

    return true;
}

bool TimeSeriesDistributed::AdvanceOneTimeStep() {
    for (auto data: m_data) {
        if (!data->AdvanceOneTimeStep()) {
            return false;
        }
    }

    return true;
}

wxDateTime TimeSeriesDistributed::GetStart() {
    wxASSERT(!m_data.empty());
    return m_data[0]->GetStart();
}

wxDateTime TimeSeriesDistributed::GetEnd() {
    wxASSERT(!m_data.empty());
    return m_data[0]->GetEnd();
}

TimeSeriesData* TimeSeriesDistributed::GetDataPointer(int unitId) {
    wxASSERT(m_data.size() == m_unitIds.size());

    for (int i = 0; i < m_data.size(); ++i) {
        if (m_unitIds[i] == unitId) {
            return m_data[i];
        }
    }

    throw ShouldNotHappen();
}