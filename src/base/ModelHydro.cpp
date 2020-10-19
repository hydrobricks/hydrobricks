
#include "ModelHydro.h"

ModelHydro::ModelHydro(SubCatchment* subCatchment, const TimeMachine& timer)
    : m_subCatchment(subCatchment),
      m_timer(timer)
{}

void ModelHydro::StartModelling() {
    return;
}

void ModelHydro::SetTimeSeries(TimeSeries* timeSeries) {
    wxASSERT(m_timeSeries[timeSeries->GetVariableType()] == nullptr);
    m_timeSeries[timeSeries->GetVariableType()] = timeSeries;
}