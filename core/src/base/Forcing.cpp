#include "Forcing.h"

Forcing::Forcing(VariableType type)
    : _type(type),
      _timeSeriesData(nullptr) {}

void Forcing::AttachTimeSeriesData(TimeSeriesData* timeSeriesData) {
    wxASSERT(timeSeriesData);
    _timeSeriesData = timeSeriesData;
}

double Forcing::GetValue() {
    wxASSERT(_timeSeriesData);
    return _timeSeriesData->GetCurrentValue();
}

bool Forcing::IsValid() const {
    // Check that time series data is attached
    if (!_timeSeriesData) {
        wxLogError(_("Forcing: No time series data attached."));
        return false;
    }

    // Check that the attached time series data is valid
    if (!_timeSeriesData->IsValid()) {
        wxLogError(_("Forcing: Attached time series data is not valid."));
        return false;
    }

    return true;
}

void Forcing::Validate() const {
    if (!IsValid()) {
        throw ModelConfigError(_("Forcing validation failed. Time series data not properly configured."));
    }
}
