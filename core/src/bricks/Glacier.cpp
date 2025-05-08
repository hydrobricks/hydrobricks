#include "Glacier.h"

Glacier::Glacier()
    : LandCover(),
      m_ice(nullptr) {
    m_ice = new IceContainer(this);
}

void Glacier::Reset() {
    m_water->Reset();
    m_ice->Reset();
}

void Glacier::SaveAsInitialState() {
    m_water->SaveAsInitialState();
    m_ice->SaveAsInitialState();
}

void Glacier::SetParameters(const BrickSettings& brickSettings) {
    Brick::SetParameters(brickSettings);
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
        m_water->AttachFluxIn(flux);
    } else {
        throw ShouldNotHappen();
    }
}

bool Glacier::IsOk() {
    if (!m_ice->IsOk()) {
        wxLogError(_("The glacier ice container is not OK (brick %s)."), m_name);
        return false;
    }
    return Brick::IsOk();
}

WaterContainer* Glacier::GetIceContainer() {
    return m_ice;
}

void Glacier::Finalize() {
    m_ice->Finalize();
    m_water->Finalize();
}

void Glacier::SetInitialState(double value, const string& type) {
    if (type == "water") {
        m_water->SetInitialState(value);
    } else if (type == "ice") {
        m_ice->SetInitialState(value);
    } else {
        throw InvalidArgument(wxString::Format(_("The content type '%s' is not supported for glaciers."), type));
    }
}

void Glacier::UpdateContent(double value, const string& type) {
    if (type == "water") {
        m_water->UpdateContent(value);
    } else if (type == "ice") {
        m_ice->UpdateContent(value);
    } else {
        throw InvalidArgument(wxString::Format(_("The content type '%s' is not supported for glaciers."), type));
    }
}

void Glacier::UpdateContentFromInputs() {
    m_ice->AddAmountToDynamicContentChange(m_ice->SumIncomingFluxes());
    m_water->AddAmountToDynamicContentChange(m_water->SumIncomingFluxes());
}

void Glacier::ApplyConstraints(double timeStep) {
    m_ice->ApplyConstraints(timeStep);
    m_water->ApplyConstraints(timeStep);
}

vecDoublePt Glacier::GetDynamicContentChanges() {
    vecDoublePt vars;
    for (auto const& var : m_water->GetDynamicContentChanges()) {
        vars.push_back(var);
    }
    for (auto const& var : m_ice->GetDynamicContentChanges()) {
        vars.push_back(var);
    }

    return vars;
}

double* Glacier::GetValuePointer(const string& name) {
    if (name == "ice" || name == "ice_content") {
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
