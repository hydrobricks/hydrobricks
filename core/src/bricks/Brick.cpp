#include "Brick.h"

#include "BrickTypes.h"
#include "ContentTypes.h"
#include "GenericLandCover.h"
#include "Glacier.h"
#include "HydroUnit.h"
#include "Snowpack.h"
#include "Storage.h"
#include "Urban.h"
#include "Vegetation.h"

Brick::Brick()
    : _needsSolver(true),
      _category(BrickCategory::Unknown),
      _water(nullptr),
      _hydroUnit(nullptr) {
    _water = std::make_unique<WaterContainer>(this);
}

Brick* Brick::Factory(const BrickSettings& brickSettings) {
    BrickType type = BrickTypeFromString(brickSettings.type);
    if (type == BrickType::Unknown) {
        wxLogError(_("Brick type '%s' not recognized. %s"), brickSettings.type, GetBrickTypeSuggestions());
        return nullptr;
    }
    Brick* brick = Factory(type);
    return brick;  // Already logged on failure inside enum factory if any
}

Brick* Brick::Factory(BrickType type) {
    switch (type) {
        case BrickType::Storage:
            return new Storage();
        case BrickType::GenericLandCover:
            return new GenericLandCover();
        case BrickType::Glacier:
            return new Glacier();
        case BrickType::Urban:
            return new Urban();
        case BrickType::Vegetation:
            return new Vegetation();
        case BrickType::Snowpack:
            return new Snowpack();
        default:
            wxLogError(_("Brick type enum not recognized."));
            return nullptr;
    }
}

void Brick::Reset() {
    _water->Reset();
    for (const auto& process : _processes) {
        process->Reset();
    }
}

void Brick::SaveAsInitialState() {
    _water->SaveAsInitialState();
}

bool Brick::IsOk() const {
    if (_processes.empty()) {
        wxLogError(_("The brick %s has no process attached"), _name);
        return false;
    }
    for (const auto& process : _processes) {
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
    if (flux->GetType() != ContentType::Water) {
        throw ModelConfigError(
            wxString::Format(_("The flux type '%s' should be water."), ContentTypeToString(flux->GetType())));
    }
    _water->AttachFluxIn(flux);
}

bool Brick::HasParameter(const BrickSettings& brickSettings, const string& name) {
    return std::any_of(brickSettings.parameters.begin(), brickSettings.parameters.end(),
                       [&name](const std::unique_ptr<Parameter>& parameter) { return parameter->GetName() == name; });
}

float* Brick::GetParameterValuePointer(const BrickSettings& brickSettings, const string& name) {
    for (const auto& parameter : brickSettings.parameters) {
        if (parameter->GetName() == name) {
            wxASSERT(parameter->GetValuePointer());
            parameter->SetAsLinked();
            return parameter->GetValuePointer();
        }
    }

    throw ModelConfigError(wxString::Format(_("The parameter '%s' could not be found."), name));
}

Process* Brick::GetProcess(size_t index) const {
    wxASSERT(_processes.size() > index);
    wxASSERT(_processes[index]);

    return _processes[index].get();
}

void Brick::Finalize() {
    _water->Finalize();
}

void Brick::SetInitialState(double value, ContentType type) {
    switch (type) {
        case ContentType::Water:
            _water->SetInitialState(value);
            break;
        default:
            throw ModelConfigError(
                wxString::Format(_("The content type '%s' is not supported."), ContentTypeToString(type)));
    }
}

double Brick::GetContent(ContentType type) const {
    switch (type) {
        case ContentType::Water:
            return _water->GetContentWithoutChanges();
        default:
            throw ModelConfigError(
                wxString::Format(_("The content type '%s' is not supported."), ContentTypeToString(type)));
    }
}

void Brick::UpdateContent(double value, ContentType type) {
    switch (type) {
        case ContentType::Water:
            _water->UpdateContent(value);
            break;
        default:
            throw ModelConfigError(
                wxString::Format(_("The content type '%s' is not supported."), ContentTypeToString(type)));
    }
}

void Brick::UpdateContentFromInputs() {
    _water->AddAmountToDynamicContentChange(_water->SumIncomingFluxes());
}

void Brick::ApplyConstraints(double timeStep) {
    _water->ApplyConstraints(timeStep);
}

WaterContainer* Brick::GetWaterContainer() const {
    return _water.get();
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

int Brick::GetProcessConnectionCount() const {
    int counter = 0;

    for (auto const& process : _processes) {
        counter += process->GetConnectionCount();
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
