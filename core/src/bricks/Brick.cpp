#include "Brick.h"

#include "GenericSurface.h"
#include "Glacier.h"
#include "HydroUnit.h"
#include "Snowpack.h"
#include "Storage.h"
#include "Surface.h"
#include "Urban.h"
#include "Vegetation.h"

Brick::Brick()
    : m_needsSolver(true),
      m_container(nullptr) {
    m_container = new WaterContainer(this);
}

Brick::~Brick() {
    wxDELETE(m_container);
}

Brick* Brick::Factory(const BrickSettings& brickSettings) {
    if (brickSettings.type == "Storage") {
        return new Storage();
    } else if (brickSettings.type == "Surface") {
        return new Surface();
    } else if (brickSettings.type == "GenericSurface") {
        return new GenericSurface();
    } else if (brickSettings.type == "Glacier") {
        return new Glacier();
    } else if (brickSettings.type == "Snowpack") {
        return new Snowpack();
    } else if (brickSettings.type == "Urban") {
        return new Urban();
    } else if (brickSettings.type == "Vegetation") {
        return new Vegetation();
    } else {
        wxLogError(_("Brick type '%s' not recognized."), brickSettings.type);
    }

    return nullptr;
}

bool Brick::IsOk() {
    for (auto process : m_processes) {
        if (!process->IsOk()) {
            return false;
        }
    }

    return true;
}

void Brick::AssignParameters(const BrickSettings& brickSettings) {
    if (HasParameter(brickSettings, "capacity")) {
        m_container->SetMaximumCapacity(GetParameterValuePointer(brickSettings, "capacity"));
    }
}

void Brick::AttachFluxIn(Flux* flux) {
    wxASSERT(flux);
    m_container->AttachFluxIn(flux);
}

bool Brick::HasParameter(const BrickSettings& brickSettings, const std::string& name) {
    for (auto parameter : brickSettings.parameters) {
        if (parameter->GetName() == name) {
            return true;
        }
    }

    return false;
}

float* Brick::GetParameterValuePointer(const BrickSettings& brickSettings, const std::string& name) {
    for (auto parameter : brickSettings.parameters) {
        if (parameter->GetName() == name) {
            wxASSERT(parameter->GetValuePointer());
            parameter->SetAsLinked();
            return parameter->GetValuePointer();
        }
    }

    throw MissingParameter(wxString::Format(_("The parameter '%s' could not be found."), name));
}

Process* Brick::GetProcess(int index) {
    wxASSERT(m_processes.size() > index);
    wxASSERT(m_processes[index]);

    return m_processes[index];
}

void Brick::Finalize() {
    m_container->Finalize();
}

void Brick::UpdateContentFromInputs() {
    m_container->AddAmount(m_container->SumIncomingFluxes());
}

void Brick::ApplyConstraints(double timeStep, bool inSolver) {
    m_container->ApplyConstraints(timeStep, inSolver);
}

WaterContainer* Brick::GetWaterContainer() {
    return m_container;
}

vecDoublePt Brick::GetStateVariableChanges() {
    return m_container->GetStateVariableChanges();
}

vecDoublePt Brick::GetStateVariableChangesFromProcesses() {
    vecDoublePt values;
    for (auto const& process : m_processes) {
        vecDoublePt processValues = process->GetStateVariables();

        if (processValues.empty()) {
            continue;
        }
        for (auto const& value : processValues) {
            values.push_back(value);
        }
    }

    return values;
}

int Brick::GetProcessesConnectionsNb() {
    int counter = 0;

    for (auto const& process : m_processes) {
        counter += process->GetConnectionsNb();
    }

    return counter;
}

double* Brick::GetBaseValuePointer(const std::string& name) {
    if (name == "content" && m_container) {
        return m_container->GetContentPointer();
    }

    return nullptr;
}

double* Brick::GetValuePointer(const std::string&) {
    return nullptr;
}
