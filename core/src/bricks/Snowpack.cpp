#include "Snowpack.h"

Snowpack::Snowpack()
    : SurfaceComponent(),
      m_snow(nullptr) {
    m_snow = new WaterContainer(this);
}

void Snowpack::AssignParameters(const BrickSettings& brickSettings) {
    Brick::AssignParameters(brickSettings);
}

void Snowpack::AttachFluxIn(Flux* flux) {
    wxASSERT(flux);
    if (flux->GetType() == "snow") {
        m_snow->AttachFluxIn(flux);
    } else if (flux->GetType() == "water") {
        m_container->AttachFluxIn(flux);
    } else {
        throw ShouldNotHappen();
    }
}

WaterContainer* Snowpack::GetSnowContainer() {
    return m_snow;
}

void Snowpack::Finalize() {
    m_snow->Finalize();
    m_container->Finalize();
}

void Snowpack::UpdateContentFromInputs() {
    m_snow->AddAmount(m_snow->SumIncomingFluxes());
    m_container->AddAmount(m_container->SumIncomingFluxes());
}

void Snowpack::ApplyConstraints(double timeStep, bool inSolver) {
    m_snow->ApplyConstraints(timeStep, inSolver);
    m_container->ApplyConstraints(timeStep, inSolver);
}

vecDoublePt Snowpack::GetStateVariableChanges() {
    vecDoublePt vars;
    for (auto const& var : m_container->GetStateVariableChanges()) {
        vars.push_back(var);
    }
    for (auto const& var : m_snow->GetStateVariableChanges()) {
        vars.push_back(var);
    }

    return vars;
}

double* Snowpack::GetValuePointer(const std::string& name) {
    if (name == "snow") {
        return m_snow->GetContentPointer();
    }

    return nullptr;
}

bool Snowpack::HasSnow() {
    return m_snow->IsNotEmpty();
}
