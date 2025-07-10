#include "TimeSeriesUniform.h"

TimeSeriesUniform::TimeSeriesUniform(VariableType type)
    : TimeSeries(type),
      _data(nullptr) {}

TimeSeriesUniform::~TimeSeriesUniform() {
    wxDELETE(_data);
}

bool TimeSeriesUniform::SetCursorToDate(double date) {
    wxASSERT(_data);
    if (!_data->SetCursorToDate(date)) {
        return false;
    }

    return true;
}

bool TimeSeriesUniform::AdvanceOneTimeStep() {
    wxASSERT(_data);
    if (!_data->AdvanceOneTimeStep()) {
        return false;
    }

    return true;
}

double TimeSeriesUniform::GetStart() {
    wxASSERT(_data);
    return _data->GetStart();
}

double TimeSeriesUniform::GetEnd() {
    wxASSERT(_data);
    return _data->GetEnd();
}

double TimeSeriesUniform::GetTotal(const SettingsBasin*) {
    throw NotImplemented();
}

TimeSeriesData* TimeSeriesUniform::GetDataPointer(int) {
    wxASSERT(_data);
    return _data;
}