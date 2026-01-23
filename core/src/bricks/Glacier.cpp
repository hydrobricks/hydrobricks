#include "Glacier.h"

Glacier::Glacier()
    : LandCover(),
      _ice(std::make_unique<IceContainer>(this)) {
    _category = BrickCategory::Glacier;
}

void Glacier::Reset() {
    _water->Reset();
    _ice->Reset();
}

void Glacier::SaveAsInitialState() {
    _water->SaveAsInitialState();
    _ice->SaveAsInitialState();
}

void Glacier::SetParameters(const BrickSettings& brickSettings) {
    Brick::SetParameters(brickSettings);
    if (HasParameter(brickSettings, "infinite_storage")) {
        const float* valPtr = GetParameterValuePointer(brickSettings, "infinite_storage");
        if (valPtr && !NearlyZero(*valPtr, EPSILON_F)) {
            _ice->SetAsInfiniteStorage();
        }
    }

    if (HasParameter(brickSettings, "no_melt_when_snow_cover")) {
        _ice->SetNoMeltWhenSnowCover(GetParameterValuePointer(brickSettings, "no_melt_when_snow_cover"));
    }
}

void Glacier::AttachFluxIn(Flux* flux) {
    wxASSERT(flux);
    if (flux->GetType() == ContentType::Ice) {
        _ice->AttachFluxIn(flux);
    } else if (flux->GetType() == ContentType::Water) {
        _water->AttachFluxIn(flux);
    } else {
        throw ShouldNotHappen(
            wxString::Format("Glacier::AttachFluxIn - Unexpected flux type: %d", static_cast<int>(flux->GetType())));
    }
}

bool Glacier::IsValid(bool checkProcesses) const {
    if (!_ice->IsValid(checkProcesses)) {
        wxLogError(_("The glacier ice container is not OK (brick %s)."), _name);
        return false;
    }
    if (checkProcesses) {
        if (_processes.empty()) {
            wxLogError(_("The brick %s has no process attached"), _name);
            return false;
        }
        for (const auto& process : _processes) {
            if (!process->IsValid()) {
                return false;
            }
        }
    }
    // We skip water container validation as glaciers may not have water processes.
    return true;
}

WaterContainer* Glacier::GetIceContainer() const {
    return _ice.get();
}

void Glacier::Finalize() {
    _ice->Finalize();
    _water->Finalize();
}

void Glacier::SetInitialState(double value, ContentType type) {
    switch (type) {
        case ContentType::Water:
            _water->SetInitialState(value);
            break;
        case ContentType::Ice:
            _ice->SetInitialState(value);
            break;
        default:
            throw ModelConfigError(
                wxString::Format(_("The content type '%s' is not supported for glaciers."), ContentTypeToString(type)));
    }
}

double Glacier::GetContent(ContentType type) const {
    switch (type) {
        case ContentType::Water:
            return _water->GetContentWithoutChanges();
        case ContentType::Ice:
            return _ice->GetContentWithoutChanges();
        default:
            throw ModelConfigError(
                wxString::Format(_("The content type '%s' is not supported for glaciers."), ContentTypeToString(type)));
    }
}

void Glacier::UpdateContent(double value, ContentType type) {
    switch (type) {
        case ContentType::Water:
            _water->UpdateContent(value);
            break;
        case ContentType::Ice:
            _ice->UpdateContent(value);
            break;
        default:
            throw ModelConfigError(
                wxString::Format(_("The content type '%s' is not supported for glaciers."), ContentTypeToString(type)));
    }
}

void Glacier::UpdateContentFromInputs() {
    _ice->AddAmountToDynamicContentChange(_ice->SumIncomingFluxes());
    _water->AddAmountToDynamicContentChange(_water->SumIncomingFluxes());
}

void Glacier::ApplyConstraints(double timeStep) {
    _ice->ApplyConstraints(timeStep);
    _water->ApplyConstraints(timeStep);
}

vecDoublePt Glacier::GetDynamicContentChanges() {
    vecDoublePt vars;
    for (auto const& var : _water->GetDynamicContentChanges()) {
        vars.push_back(var);
    }
    for (auto const& var : _ice->GetDynamicContentChanges()) {
        vars.push_back(var);
    }

    return vars;
}

double* Glacier::GetValuePointer(const string& name) {
    if (name == "ice" || name == "ice_content") {
        return _ice->GetContentPointer();
    }

    return nullptr;
}

void Glacier::SurfaceComponentAdded(SurfaceComponent* brick) {
    if (brick->GetCategory() == BrickCategory::Snowpack) {
        auto snowpack = dynamic_cast<Snowpack*>(brick);
        _ice->SetRelatedSnowpack(snowpack);
    }
}

bool Glacier::HasIce() const {
    return _ice->IsNotEmpty();
}
