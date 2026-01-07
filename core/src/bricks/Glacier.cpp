#include "Glacier.h"

Glacier::Glacier()
    : LandCover(),
      _ice(nullptr) {
    _ice = new IceContainer(this);
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
        float* valPtr = GetParameterValuePointer(brickSettings, "infinite_storage");
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
        throw ShouldNotHappen();
    }
}

bool Glacier::IsOk() {
    if (!_ice->IsOk()) {
        wxLogError(_("The glacier ice container is not OK (brick %s)."), _name);
        return false;
    }
    return Brick::IsOk();
}

WaterContainer* Glacier::GetIceContainer() {
    return _ice;
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
            throw InvalidArgument(
                wxString::Format(_("The content type '%s' is not supported for glaciers."), ContentTypeToString(type)));
    }
}

double Glacier::GetContent(ContentType type) {
    switch (type) {
        case ContentType::Water:
            return _water->GetContentWithoutChanges();
        case ContentType::Ice:
            return _ice->GetContentWithoutChanges();
        default:
            throw InvalidArgument(
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
            throw InvalidArgument(
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
    if (brick->IsSnowpack()) {
        auto snowpack = dynamic_cast<Snowpack*>(brick);
        _ice->SetRelatedSnowpack(snowpack);
    }
}
