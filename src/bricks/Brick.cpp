#include "Brick.h"

#include "GenericSurface.h"
#include "Glacier.h"
#include "HydroUnit.h"
#include "Snowpack.h"
#include "Storage.h"
#include "Surface.h"
#include "Urban.h"
#include "Vegetation.h"

Brick::Brick(HydroUnit *hydroUnit, bool withWaterContainer)
    : m_needsSolver(true),
      m_container(nullptr),
      m_hydroUnit(hydroUnit)
{
    if (hydroUnit) {
        hydroUnit->AddBrick(this);
    }

    if (withWaterContainer) {
        m_container = new WaterContainer(this);
    }
}

Brick::~Brick() {
    wxDELETE(m_container);
}

Brick* Brick::Factory(const BrickSettings &brickSettings, HydroUnit* unit) {
    if (brickSettings.type.IsSameAs("Storage")) {
        auto brick = new Storage(unit);
        brick->AssignParameters(brickSettings);
        return brick;
    } else if (brickSettings.type.IsSameAs("Surface")) {
        auto brick = new Surface(unit);
        brick->AssignParameters(brickSettings);
        return brick;
    } else if (brickSettings.type.IsSameAs("GenericSurface")) {
        auto brick = new GenericSurface(unit);
        brick->AssignParameters(brickSettings);
        return brick;
    } else if (brickSettings.type.IsSameAs("Glacier")) {
        auto brick = new Glacier(unit);
        brick->AssignParameters(brickSettings);
        return brick;
    } else if (brickSettings.type.IsSameAs("Snowpack")) {
        auto brick = new Snowpack(unit);
        brick->AssignParameters(brickSettings);
        return brick;
    } else if (brickSettings.type.IsSameAs("Urban")) {
        auto brick = new Urban(unit);
        brick->AssignParameters(brickSettings);
        return brick;
    } else if (brickSettings.type.IsSameAs("Vegetation")) {
        auto brick = new Vegetation(unit);
        brick->AssignParameters(brickSettings);
        return brick;
    } else {
        wxLogError(_("Brick type '%s' not recognized."), brickSettings.type);
    }

    return nullptr;
}

bool Brick::IsOk() {
    if (m_hydroUnit == nullptr) {
        wxLogError(_("The brick is not attached to a hydro unit."));
        return false;
    }
    for (auto process : m_processes) {
        if (!process->IsOk()) {
            return false;
        }
    }

    return true;
}

void Brick::AssignParameters(const BrickSettings &brickSettings) {
    if (HasParameter(brickSettings, "capacity")) {
        CheckWaterContainer();
        m_container->SetMaximumCapacity(GetParameterValuePointer(brickSettings, "capacity"));
    }
}

bool Brick::HasParameter(const BrickSettings &brickSettings, const wxString &name) {
    for (auto parameter: brickSettings.parameters) {
        if (parameter->GetName().IsSameAs(name, false)) {
            return true;
        }
    }

    return false;
}

float* Brick::GetParameterValuePointer(const BrickSettings &brickSettings, const wxString &name) {
    for (auto parameter: brickSettings.parameters) {
        if (parameter->GetName().IsSameAs(name, false)) {
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

void Brick::SubtractAmount(double change) {
    if (HasWaterContainer()) {
        m_container->SubtractAmount(change);
    }
}

void Brick::AddAmount(double change) {
    CheckWaterContainer();
    m_container->AddAmount(change);
}

void Brick::Finalize() {
    CheckWaterContainer();
    m_container->Finalize();
}

double Brick::SumIncomingFluxes() {
    double sum = 0;
    for (auto & input : m_inputs) {
        sum += input->GetAmount();
    }

    return sum;
}

void Brick::UpdateContentFromInputs() {
    if (HasWaterContainer()) {
        m_container->AddAmount(SumIncomingFluxes());
    }
}

void Brick::ApplyConstraints(double timeStep) {
    if (HasWaterContainer()) {
        m_container->ApplyConstraints(timeStep);
    }
}

void Brick::CheckWaterContainer() {
    if (!HasWaterContainer()) {
        throw ConceptionIssue(_("Trying to access the water container of a brick that has none."));
    }
}

bool Brick::HasWaterContainer() {
    return m_container != nullptr;
}

WaterContainer* Brick::GetWaterContainer() {
    CheckWaterContainer();
    return m_container;
}

vecDoublePt Brick::GetStateVariableChanges() {
    if (HasWaterContainer()) {
        return m_container->GetStateVariableChanges();
    }
    return vecDoublePt {};
}

vecDoublePt Brick::GetStateVariableChangesFromProcesses() {
    vecDoublePt values;
    for (auto const &process: m_processes) {
        vecDoublePt processValues = process->GetStateVariables();

        if (processValues.empty()) {
            continue;
        }
        for (auto const &value : processValues) {
            values.push_back(value);
        }
    }

    return values;
}

int Brick::GetProcessesConnectionsNb() {
    int counter = 0;

    for (auto const &process: m_processes) {
        counter += process->GetConnectionsNb();
    }

    return counter;
}

double* Brick::GetBaseValuePointer(const wxString& name) {
    if (name.IsSameAs("content") && m_container) {
        return m_container->GetContentPointer();
    }

    return nullptr;
}

double* Brick::GetValuePointer(const wxString&) {
    return nullptr;
}
