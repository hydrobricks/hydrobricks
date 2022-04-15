#include "ModelHydro.h"

ModelHydro::ModelHydro(SubBasin* subBasin, const TimeMachine& timer)
    : m_subBasin(subBasin),
      m_timer(timer)
{}

bool ModelHydro::IsOk() {
    if (!m_subBasin->IsOk()) return false;

    return true;
}

void ModelHydro::StartModelling() {
    //
}

void ModelHydro::SetTimeSeries(TimeSeries* timeSeries) {
    wxASSERT(m_timeSeries[timeSeries->GetVariableType()] == nullptr);
    m_timeSeries[timeSeries->GetVariableType()] = timeSeries;
}