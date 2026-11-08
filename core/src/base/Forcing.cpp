#include "Forcing.h"

Forcing::Forcing(VariableType type)
    : _type(type),
      _timeSeriesData(nullptr) {}

void Forcing::AttachTimeSeriesData(TimeSeriesData* timeSeriesData) {
    assert(timeSeriesData);
    _timeSeriesData = timeSeriesData;
}

double Forcing::GetValue() const {
    assert(_timeSeriesData);
    if (_hasUpdatedValue) return _updatedValue;
    return _timeSeriesData->GetCurrentValue();
}

void Forcing::UpdateValue(double value) {
    _updatedValue = value;
    _hasUpdatedValue = true;
}

void Forcing::ResetUpdate() {
    _hasUpdatedValue = false;
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

AggregatedForcing::AggregatedForcing(VariableType type)
    : Forcing(type) {}

void AggregatedForcing::AddSource(Forcing* source, double weight) {
    assert(source);
    assert(source->GetType() == _type);
    _sources.emplace_back(source, weight);
}

double AggregatedForcing::GetValue() const {
    if (_hasUpdatedValue) return _updatedValue;
    double value = 0;
    for (const auto& [source, weight] : _sources) {
        value += weight * source->GetValue();
    }
    return value;
}

bool AggregatedForcing::IsValid() const {
    if (_sources.empty()) {
        LogError("Aggregated forcing: no hydro unit forcing to aggregate.");
        return false;
    }
    for (const auto& [source, weight] : _sources) {
        if (!source->IsValid()) {
            return false;
        }
    }

    return true;
}
