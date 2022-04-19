#include "ModelHydro.h"

ModelHydro::ModelHydro(Processor* processor, SubBasin* subBasin, TimeMachine* timer)
    : m_processor(processor),
      m_subBasin(subBasin),
      m_timer(timer)
{}

bool ModelHydro::IsOk() {
    if (!m_subBasin->IsOk()) return false;
    if (m_processor == nullptr) {
        wxLogError(_("The processor was not created."));
        return false;
    }
    if (m_timer == nullptr) {
        wxLogError(_("The timer was not created."));
        return false;
    }

    return true;
}

void ModelHydro::StartModelling() {
    //
}

void ModelHydro::SetTimeSeries(TimeSeries* timeSeries) {
    wxASSERT(m_timeSeries[timeSeries->GetVariableType()] == nullptr);
    m_timeSeries[timeSeries->GetVariableType()] = timeSeries;
}