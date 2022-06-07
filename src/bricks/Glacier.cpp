#include "Glacier.h"

Glacier::Glacier(HydroUnit *hydroUnit)
    : SurfaceComponent(hydroUnit),
      m_ice(nullptr),
      m_infiniteStorage(true)
{
    m_ice = new WaterContainer(this);
}

void Glacier::AssignParameters(const BrickSettings &brickSettings) {
    Brick::AssignParameters(brickSettings);
}

void Glacier::AttachFluxIn(Flux* flux) {
    wxASSERT(flux);
    if (flux->GetType().IsSameAs("ice")) {
        m_ice->AttachFluxIn(flux);
    } else if (flux->GetType().IsSameAs("water")) {
        m_container->AttachFluxIn(flux);
    } else {
        throw ShouldNotHappen();
    }
}

WaterContainer* Glacier::GetIceContainer() {
    return m_ice;
}

void Glacier::Finalize() {
    if (!m_infiniteStorage) {
        m_ice->Finalize();
    }
    m_container->Finalize();
}

void Glacier::UpdateContentFromInputs() {
    if (!m_infiniteStorage) {
        m_ice->AddAmount(m_ice->SumIncomingFluxes());
    }
    m_container->AddAmount(m_container->SumIncomingFluxes());
}

void Glacier::ApplyConstraints(double timeStep) {
    if (!m_infiniteStorage) {
        m_ice->ApplyConstraints(timeStep);
    }
    m_container->ApplyConstraints(timeStep);
}

vecDoublePt Glacier::GetStateVariableChanges() {
    vecDoublePt vars;
    for (auto const &var: m_container->GetStateVariableChanges()) {
        vars.push_back(var);
    }
    for (auto const &var: m_ice->GetStateVariableChanges()) {
        vars.push_back(var);
    }

    return vars;
}

double* Glacier::GetValuePointer(const wxString& name) {
    if (name.IsSameAs("ice")) {
        return m_ice->GetContentPointer();
    }

    return nullptr;
}
