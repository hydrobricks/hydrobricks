#include "Glacier.h"

Glacier::Glacier()
    : SurfaceComponent(),
      m_ice(nullptr),
      m_noMeltWhenSnowCover(false) {
    m_ice = new WaterContainer(this);
}

void Glacier::AssignParameters(const BrickSettings& brickSettings) {
    Brick::AssignParameters(brickSettings);
    if (HasParameter(brickSettings, "infiniteStorage")) {
        if (GetParameterValuePointer(brickSettings, "infiniteStorage")) {
            m_ice->SetAsInfiniteStorage();
        }
    } else {
        m_ice->SetAsInfiniteStorage();
        wxLogWarning(_("No option found to specify if the glacier has an infinite storage. Defaulting to true."));
        // wxLogWarning(_("The glacier has no infinite storage. An initial water equivalent must be provided."));
    }

    if (HasParameter(brickSettings, "noMeltWhenSnowCover")) {
        m_noMeltWhenSnowCover = GetParameterValuePointer(brickSettings, "noMeltWhenSnowCover");
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

void Glacier::ApplyConstraints(double timeStep, bool inSolver) {
    if (m_noMeltWhenSnowCover) {
        if (m_snowpack == nullptr) {
            throw ConceptionIssue(_("No snowpack provided for the glacier melt limitation."));
        }
        if (m_snowpack->HasSnow()) {
            m_ice->SetOutgoingRatesToZero();
        }
    }
    m_ice->ApplyConstraints(timeStep, inSolver);
    m_container->ApplyConstraints(timeStep, inSolver);
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

void Glacier::AddToRelatedBricks(SurfaceComponent* brick) {
    m_relatedBricks.push_back(brick);
    if (m_noMeltWhenSnowCover && brick->IsSnowpack()) {
        m_snowpack = dynamic_cast<Snowpack*>(brick);
    }
}
