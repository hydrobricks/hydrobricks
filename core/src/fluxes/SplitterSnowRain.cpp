#include "SplitterSnowRain.h"

SplitterSnowRain::SplitterSnowRain()
    : Splitter()
{}

bool SplitterSnowRain::IsOk() {
    if (m_outputs.size() != 2) {
        wxLogError(_("SplitterSnowRain should have 2 outputs."));
        return false;
    }

    return true;
}

void SplitterSnowRain::AssignParameters(const SplitterSettings &splitterSettings) {
    m_transitionStart = GetParameterValuePointer(splitterSettings, "transitionStart");
    m_transitionEnd = GetParameterValuePointer(splitterSettings, "transitionEnd");
}

void SplitterSnowRain::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == Precipitation) {
        m_precipitation = forcing;
    } else if (forcing->GetType() == Temperature) {
        m_temperature = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type Temperature or Precipitation");
    }
}

double* SplitterSnowRain::GetValuePointer(const std::string& name) {
    if (name == "rain") {
        return m_outputs[0]->GetAmountPointer();
    } else if (name == "snow") {
        return m_outputs[1]->GetAmountPointer();
    }

    return nullptr;
}

void SplitterSnowRain::Compute() {
    if (m_temperature->GetValue() <= *m_transitionStart) {
        m_outputs[0]->UpdateFlux(0);
        m_outputs[1]->UpdateFlux(m_precipitation->GetValue());
    } else if (m_temperature->GetValue() >= *m_transitionEnd) {
        m_outputs[0]->UpdateFlux(m_precipitation->GetValue());
        m_outputs[1]->UpdateFlux(0);
    } else {
        double rainFraction = (m_temperature->GetValue() - *m_transitionStart) / (*m_transitionEnd - *m_transitionStart);
        m_outputs[0]->UpdateFlux(m_precipitation->GetValue() * rainFraction);
        m_outputs[1]->UpdateFlux(m_precipitation->GetValue() * (1 - rainFraction));
    }
}