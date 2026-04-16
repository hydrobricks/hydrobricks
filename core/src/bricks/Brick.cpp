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
        LogError("Brick type '{}' not recognized. {}", brickSettings.type, GetBrickTypeSuggestions());
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
            LogError("Brick type enum not recognized.");
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

bool Brick::IsValid(bool checkProcesses) const {
    if (checkProcesses) {
        if (_processes.empty()) {
            LogError("The brick {} has no process attached", _name);
            return false;
        }
        for (const auto& process : _processes) {
            if (!process->IsValid()) {
                return false;
            }
        }
    }
    return _water->IsValid(checkProcesses);
}

void Brick::Validate() const {
    if (!IsValid()) {
        throw ModelConfigError(
            std::format("The brick {} validation failed. Check that all processes are properly configured.", _name));
    }
}

void Brick::SetParameters(const BrickSettings& brickSettings) {
    if (HasParameter(brickSettings, "capacity")) {
        _water->SetMaximumCapacity(GetParameterValuePointer(brickSettings, "capacity"));
    }
}

void Brick::AttachFluxIn(Flux* flux) {
    assert(flux);
    if (flux->GetType() != ContentType::Water) {
        throw ModelConfigError(
            std::format("The flux type '{}' should be water.", ContentTypeToString(flux->GetType())));
    }
    _water->AttachFluxIn(flux);
}

bool Brick::HasParameter(const BrickSettings& brickSettings, const string& name) {
    return std::any_of(brickSettings.parameters.begin(), brickSettings.parameters.end(),
                       [&name](const Parameter& parameter) { return parameter.GetName() == name; });
}

const float* Brick::GetParameterValuePointer(const BrickSettings& brickSettings, const string& name) {
    for (auto& parameter : brickSettings.parameters) {
        if (parameter.GetName() == name) {
            assert(parameter.GetValuePointer());
            return parameter.GetValuePointer();
        }
    }

    throw ModelConfigError(std::format("The parameter '{}' could not be found.", name));
}

Process* Brick::GetProcess(size_t index) const {
    assert(_processes.size() > index);
    assert(_processes[index]);

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
            throw ModelConfigError(std::format("The content type '{}' is not supported.", ContentTypeToString(type)));
    }
}

double Brick::GetContent(ContentType type) const {
    switch (type) {
        case ContentType::Water:
            return _water->GetContentWithoutChanges();
        default:
            throw ModelConfigError(std::format("The content type '{}' is not supported.", ContentTypeToString(type)));
    }
}

void Brick::UpdateContent(double value, ContentType type) {
    switch (type) {
        case ContentType::Water:
            _water->UpdateContent(value);
            break;
        default:
            throw ModelConfigError(std::format("The content type '{}' is not supported.", ContentTypeToString(type)));
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
