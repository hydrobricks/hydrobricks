#include "Brick.h"

#include "GenericLandCover.h"
#include "Glacier.h"
#include "HydroUnit.h"
#include "Snowpack.h"
#include "Storage.h"
#include "Urban.h"
#include "Vegetation.h"

Brick::Brick()
    : m_needsSolver(true),
      m_water(nullptr) {
    m_water = new WaterContainer(this);
}

Brick::~Brick() {
    wxDELETE(m_water);
}

Brick* Brick::Factory(const BrickSettings& brickSettings) {
    if (brickSettings.type == "storage") {
        return new Storage();
    } else if (brickSettings.type == "generic_land_cover" || brickSettings.type == "ground") {
        return new GenericLandCover();
    } else if (brickSettings.type == "glacier") {
        return new Glacier();
    } else if (brickSettings.type == "urban") {
        return new Urban();
    } else if (brickSettings.type == "vegetation") {
        return new Vegetation();
    } else if (brickSettings.type == "snowpack") {
        return new Snowpack();
    } else {
        wxLogError(_("Brick type '%s' not recognized."), brickSettings.type);
    }

    return nullptr;
}

void Brick::Reset() {
    m_water->Reset();
    for (auto process : m_processes) {
        process->Reset();
    }
}

void Brick::SaveAsInitialState() {
    m_water->SaveAsInitialState();
}

bool Brick::IsOk() {
    if (m_processes.empty()) {
        wxLogError(_("The brick %s has no process attached"), m_name);
        return false;
    }
    for (auto process : m_processes) {
        if (!process->IsOk()) {
            return false;
        }
    }
    return m_water->IsOk();
}

void Brick::SetParameters(const BrickSettings& brickSettings) {
    if (HasParameter(brickSettings, "capacity")) {
        m_water->SetMaximumCapacity(GetParameterValuePointer(brickSettings, "capacity"));
    }
}

void Brick::AttachFluxIn(Flux* flux) {
    wxASSERT(flux);
    if (flux->GetType() != "water") {
        throw InvalidArgument(wxString::Format(_("The flux type '%s' should be water."), flux->GetType()));
    }
    m_water->AttachFluxIn(flux);
}

bool Brick::HasParameter(const BrickSettings& brickSettings, const string& name) {
    for (auto parameter : brickSettings.parameters) {
        if (parameter->GetName() == name) {
            return true;
        }
    }

    return false;
}

float* Brick::GetParameterValuePointer(const BrickSettings& brickSettings, const string& name) {
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
    m_water->Finalize();
}

void Brick::SetInitialState(double value, const string& type) {
    if (type == "water") {
        m_water->SetInitialState(value);
    } else {
        throw InvalidArgument(wxString::Format(_("The content type '%s' is not supported."), type));
    }
}

double Brick::GetContent(const string& type) {
    if (type == "water") {
        return m_water->GetContentWithoutChanges();
    }

    throw InvalidArgument(wxString::Format(_("The content type '%s' is not supported."), type));
}

void Brick::UpdateContent(double value, const string& type) {
    if (type == "water") {
        m_water->UpdateContent(value);
    } else {
        throw InvalidArgument(wxString::Format(_("The content type '%s' is not supported."), type));
    }
}

void Brick::UpdateContentFromInputs() {
    m_water->AddAmountToDynamicContentChange(m_water->SumIncomingFluxes());
}

void Brick::ApplyConstraints(double timeStep) {
    m_water->ApplyConstraints(timeStep);
}

WaterContainer* Brick::GetWaterContainer() {
    return m_water;
}

vecDoublePt Brick::GetDynamicContentChanges() {
    return m_water->GetDynamicContentChanges();
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

double* Brick::GetBaseValuePointer(const string& name) {
    if ((name == "water" || name == "water_content") && m_water) {
        return m_water->GetContentPointer();
    }

    return nullptr;
}

double* Brick::GetValuePointer(const string&) {
    return nullptr;
}
