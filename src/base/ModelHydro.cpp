
#include "ModelHydro.h"

ModelHydro::ModelHydro(SubBasin* subBasin, const TimeMachine& timer)
    : m_subBasin(subBasin),
      m_timer(timer)
{}

void ModelHydro::StartModelling() {
    return;
}

void ModelHydro::SetTimeSeries(TimeSeries* timeSeries) {
    wxASSERT(m_timeSeries[timeSeries->GetVariableType()] == nullptr);
    m_timeSeries[timeSeries->GetVariableType()] = timeSeries;
}