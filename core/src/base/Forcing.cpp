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
