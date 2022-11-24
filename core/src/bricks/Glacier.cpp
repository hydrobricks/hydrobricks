#include "Glacier.h"

Glacier::Glacier()
    : LandCover(),
      m_ice(nullptr) {
    m_ice = new IceContainer(this);
}

void Glacier::Reset() {
    m_container->Reset();
    m_ice->Reset();
}

void Glacier::SaveAsInitialState() {
    m_container->SaveAsInitialState();
    m_ice->SaveAsInitialState();
}

void Glacier::AssignParameters(const BrickSettings& brickSettings) {
    Brick::AssignParameters(brickSettings);
    if (HasParameter(brickSettings, "infinite_storage")) {
        if (GetParameterValuePointer(brickSettings, "infinite_storage")) {
            m_ice->SetAsInfiniteStorage();
        }
    } else {
        m_ice->SetAsInfiniteStorage();
        wxLogWarning(_("No option found to specify if the glacier has an infinite storage. Defaulting to true."));
        // wxLogWarning(_("The glacier has no infinite storage. An initial water equivalent must be provided."));
    }

    if (HasParameter(brickSettings, "no_melt_when_snow_cover")) {
        m_ice->SetNoMeltWhenSnowCover(GetParameterValuePointer(brickSettings, "no_melt_when_snow_cover"));
    }
}

void Glacier::AttachFluxIn(Flux* flux) {
    wxASSERT(flux);
    if (flux->GetType() == "ice") {
        m_ice->AttachFluxIn(flux);
    } else if (flux->GetType() == "water") {
        m_container->AttachFluxIn(flux);
    } else {
        throw ShouldNotHappen();
    }
}

bool Glacier::IsOk() {
    if (!m_ice->IsOk()) {
        return false;
    }
    return Brick::IsOk();
}

WaterContainer* Glacier::GetIceContainer() {
    return m_ice;
}

void Glacier::Finalize() {
    m_ice->Finalize();
    m_container->Finalize();
}

void Glacier::UpdateContentFromInputs() {
    m_ice->AddAmount(m_ice->SumIncomingFluxes());
    m_container->AddAmount(m_container->SumIncomingFluxes());
}

void Glacier::ApplyConstraints(double timeStep) {
    m_ice->ApplyConstraints(timeStep);
    m_container->ApplyConstraints(timeStep);
}

vecDoublePt Glacier::GetStateVariableChanges() {
    vecDoublePt vars;
    for (auto const& var : m_container->GetStateVariableChanges()) {
        vars.push_back(var);
    }
    for (auto const& var : m_ice->GetStateVariableChanges()) {
        vars.push_back(var);
    }

    return vars;
}

double* Glacier::GetValuePointer(const std::string& name) {
    if (name == "ice") {
        return m_ice->GetContentPointer();
    }

    return nullptr;
}

void Glacier::SurfaceComponentAdded(SurfaceComponent* brick) {
    if (brick->IsSnowpack()) {
        auto snowpack = dynamic_cast<Snowpack*>(brick);
        m_ice->SetRelatedSnowpack(snowpack);
    }
}
