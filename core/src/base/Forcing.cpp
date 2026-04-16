#include "Forcing.h"

Forcing::Forcing(VariableType type)
    : _type(type),
      _timeSeriesData(nullptr) {}

void Forcing::AttachTimeSeriesData(TimeSeriesData* timeSeriesData) {
    assert(timeSeriesData);
    _timeSeriesData = timeSeriesData;
}

double Forcing::GetValue() {
    assert(_timeSeriesData);
    return _timeSeriesData->GetCurrentValue();
}

bool Forcing::IsValid() const {
    // Check that time series data is attached
    if (!_timeSeriesData) {
        LogError("Forcing: No time series data attached.");
        return false;
    }

    // Check that the attached time series data is valid
    if (!_timeSeriesData->IsValid()) {
        LogError("Forcing: Attached time series data is not valid.");
        return false;
    }

    return true;
}

void Forcing::Validate() const {
    if (!IsValid()) {
        throw ModelConfigError("Forcing validation failed. Time series data not properly configured.");
    }
}
