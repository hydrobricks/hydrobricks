#include "ModelHydro.h"

ModelHydro::ModelHydro(Processor* processor, SubBasin* subBasin, TimeMachine* timer)
    : m_processor(processor),
      m_subBasin(subBasin),
      m_timer(timer)
{
    if (processor) {
        processor->SetModel(this);
    }
}

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

bool ModelHydro::Run() {
    while (!m_timer->IsOver()) {
        if (!m_processor->ProcessTimeStep()) {
            wxLogError(_("Failed running the model."));
            return false;
        }
        m_timer->IncrementTime();
    }
    return false;
}

void ModelHydro::SetTimeSeries(TimeSeries* timeSeries) {
    wxASSERT(m_timeSeries[timeSeries->GetVariableType()] == nullptr);
    m_timeSeries[timeSeries->GetVariableType()] = timeSeries;
}