#include "TimeSeriesUniform.h"

TimeSeriesUniform::TimeSeriesUniform(VariableType type)
    : TimeSeries(type),
      _data(nullptr) {}

TimeSeriesUniform::~TimeSeriesUniform() = default;  // Automatic cleanup via unique_ptr

bool TimeSeriesUniform::SetCursorToDate(double date) {
    assert(_data);
    if (!_data->SetCursorToDate(date)) {
        return false;
    }

    return true;
}

bool TimeSeriesUniform::AdvanceOneTimeStep() {
    assert(_data);
    if (!_data->AdvanceOneTimeStep()) {
        return false;
    }

    return true;
}

double TimeSeriesUniform::GetStart() const {
    assert(_data);
    return _data->GetStart();
}

double TimeSeriesUniform::GetEnd() const {
    assert(_data);
    return _data->GetEnd();
}

double TimeSeriesUniform::GetTotal(const SettingsBasin*) {
    throw NotImplemented("TimeSeriesUniform::GetTotal - Not yet implemented");
}

TimeSeriesData* TimeSeriesUniform::GetDataPointer(int) {
    assert(_data);
    return _data.get();
}

bool TimeSeriesUniform::IsValid() const {
    // Check that data has been set
    if (!_data) {
        LogError("TimeSeriesUniform: No data set.");
        return false;
    }

    // Check that the data is valid
    if (!_data->IsValid()) {
        LogError("TimeSeriesUniform: Data is not valid.");
        return false;
    }

    return true;
}

void TimeSeriesUniform::Validate() const {
    if (!IsValid()) {
        throw ModelConfigError("TimeSeriesUniform validation failed. Data not properly configured.");
    }
}
