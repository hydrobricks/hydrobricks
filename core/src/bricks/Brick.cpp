#include "Brick.h"

#include "GenericLandCover.h"
#include "Glacier.h"
#include "HydroUnit.h"
#include "Snowpack.h"
#include "Storage.h"
#include "Urban.h"
#include "Vegetation.h"

Brick::Brick()
    : _needsSolver(true),
      _water(nullptr),
      _hydroUnit(nullptr) {
    _water = new WaterContainer(this);
}

Brick::~Brick() {
    wxDELETE(_water);
}

Brick* Brick::Factory(const BrickSettings& brickSettings) {
    if (brickSettings.type == "storage") {
        return new Storage();
    }
    if (brickSettings.type == "generic_land_cover" || brickSettings.type == "ground") {
        return new GenericLandCover();
    }
    if (brickSettings.type == "glacier") {
        return new Glacier();
    }
    if (brickSettings.type == "urban") {
        return new Urban();
    }
    if (brickSettings.type == "vegetation") {
        return new Vegetation();
    }
    if (brickSettings.type == "snowpack") {
        return new Snowpack();
    }
    wxLogError(_("Brick type '%s' not recognized."), brickSettings.type);

    return nullptr;
}

void Brick::Reset() {
    _water->Reset();
    for (auto process : _processes) {
        process->Reset();
    }
}

void Brick::SaveAsInitialState() {
    _water->SaveAsInitialState();
}

bool Brick::IsOk() {
    if (_processes.empty()) {
        wxLogError(_("The brick %s has no process attached"), _name);
        return false;
    }
    for (auto process : _processes) {
        if (!process->IsOk()) {
            return false;
        }
    }
    return _water->IsOk();
}

void Brick::SetParameters(const BrickSettings& brickSettings) {
    if (HasParameter(brickSettings, "capacity")) {
        _water->SetMaximumCapacity(GetParameterValuePointer(brickSettings, "capacity"));
    }
}

void Brick::AttachFluxIn(Flux* flux) {
    wxASSERT(flux);
    if (flux->GetType() != "water") {
        throw InvalidArgument(wxString::Format(_("The flux type '%s' should be water."), flux->GetType()));
    }
    _water->AttachFluxIn(flux);
}

bool Brick::HasParameter(const BrickSettings& brickSettings, const string& name) {
    return std::any_of(brickSettings.parameters.begin(), brickSettings.parameters.end(),
                       [&name](const Parameter* parameter) { return parameter->GetName() == name; });
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
    wxASSERT(_processes.size() > index);
    wxASSERT(_processes[index]);

    return _processes[index];
}

void Brick::Finalize() {
    _water->Finalize();
}

void Brick::SetInitialState(double value, const string& type) {
    if (type == "water") {
        _water->SetInitialState(value);
    } else {
        throw InvalidArgument(wxString::Format(_("The content type '%s' is not supported."), type));
    }
}

double Brick::GetContent(const string& type) {
    if (type == "water") {
        return _water->GetContentWithoutChanges();
    }

    throw InvalidArgument(wxString::Format(_("The content type '%s' is not supported."), type));
}

void Brick::UpdateContent(double value, const string& type) {
    if (type == "water") {
        _water->UpdateContent(value);
    } else {
        throw InvalidArgument(wxString::Format(_("The content type '%s' is not supported."), type));
    }
}

void Brick::UpdateContentFromInputs() {
    _water->AddAmountToDynamicContentChange(_water->SumIncomingFluxes());
}

void Brick::ApplyConstraints(double timeStep) {
    _water->ApplyConstraints(timeStep);
}

WaterContainer* Brick::GetWaterContainer() {
    return _water;
}

vecDoublePt Brick::GetDynamicContentChanges() {
    return _water->GetDynamicContentChanges();
}

vecDoublePt Brick::GetStateVariableChangesFromProcesses() {
    vecDoublePt values;
    for (auto const& process : _processes) {
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

    for (auto const& process : _processes) {
        counter += process->GetConnectionsNb();
    }

    return counter;
}

double* Brick::GetBaseValuePointer(const string& name) {
    if ((name == "water" || name == "water_content") && _water) {
        return _water->GetContentPointer();
    }

    return nullptr;
}

double* Brick::GetValuePointer(const string&) {
    return nullptr;
}
