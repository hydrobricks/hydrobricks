#include "Snowpack.h"

Snowpack::Snowpack()
    : SurfaceComponent(),
      m_snow(nullptr) {
    m_snow = new SnowContainer(this);
}

void Snowpack::Reset() {
    m_water->Reset();
    m_snow->Reset();
}

void Snowpack::SaveAsInitialState() {
    m_water->SaveAsInitialState();
    m_snow->SaveAsInitialState();
}

void Snowpack::SetParameters(const BrickSettings& brickSettings) {
    Brick::SetParameters(brickSettings);
}

void Snowpack::AttachFluxIn(Flux* flux) {
    wxASSERT(flux);
    if (flux->GetType() == "snow") {
        m_snow->AttachFluxIn(flux);
    } else if (flux->GetType() == "water") {
        m_water->AttachFluxIn(flux);
    } else {
        throw ShouldNotHappen();
    }
}

bool Snowpack::IsOk() {
    if (!m_snow->IsOk()) {
        return false;
    }
    return Brick::IsOk();
}

WaterContainer* Snowpack::GetSnowContainer() {
    return m_snow;
}

void Snowpack::Finalize() {
    m_snow->Finalize();
    m_water->Finalize();
}

void Snowpack::UpdateContentFromInputs() {
    m_snow->AddAmountToStaticContentChange(m_snow->SumIncomingFluxes());
    m_water->AddAmountToDynamicContentChange(m_water->SumIncomingFluxes());
}

void Snowpack::ApplyConstraints(double timeStep) {
    m_snow->ApplyConstraints(timeStep);
    m_water->ApplyConstraints(timeStep);
}

vecDoublePt Snowpack::GetDynamicContentChanges() {
    vecDoublePt vars;
    for (auto const& var : m_water->GetDynamicContentChanges()) {
        vars.push_back(var);
    }
    for (auto const& var : m_snow->GetDynamicContentChanges()) {
        vars.push_back(var);
    }

    return vars;
}

double* Snowpack::GetValuePointer(const string& name) {
    if (name == "snow") {
        return m_snow->GetContentPointer();
    }

    return nullptr;
}

bool Snowpack::HasSnow() {
    return m_snow->IsNotEmpty();
}
