#include "Snowpack.h"

Snowpack::Snowpack()
    : SurfaceComponent(),
      _snow(nullptr) {
    _snow = new SnowContainer(this);
}

void Snowpack::Reset() {
    _water->Reset();
    _snow->Reset();
}

void Snowpack::SaveAsInitialState() {
    _water->SaveAsInitialState();
    _snow->SaveAsInitialState();
}

void Snowpack::SetParameters(const BrickSettings& brickSettings) {
    Brick::SetParameters(brickSettings);
}

void Snowpack::AttachFluxIn(Flux* flux) {
    wxASSERT(flux);
    if (flux->GetType() == ContentType::Snow) {
        _snow->AttachFluxIn(flux);
    } else if (flux->GetType() == ContentType::Water) {
        _water->AttachFluxIn(flux);
    } else {
        throw ShouldNotHappen();
    }
}

bool Snowpack::IsOk() {
    if (!_snow->IsOk()) {
        return false;
    }
    return Brick::IsOk();
}

WaterContainer* Snowpack::GetSnowContainer() {
    return _snow;
}

void Snowpack::Finalize() {
    _snow->Finalize();
    _water->Finalize();
}

void Snowpack::SetInitialState(double value, ContentType type) {
    switch (type) {
        case ContentType::Water:
            _water->SetInitialState(value);
            break;
        case ContentType::Snow:
            _snow->SetInitialState(value);
            break;
        default:
            throw InvalidArgument(
                wxString::Format(_("The content type '%s' is not supported for snowpack."), ContentTypeToString(type)));
    }
}

double Snowpack::GetContent(ContentType type) {
    switch (type) {
        case ContentType::Water:
            return _water->GetContentWithoutChanges();
        case ContentType::Snow:
            return _snow->GetContentWithoutChanges();
        default:
            throw InvalidArgument(
                wxString::Format(_("The content type '%s' is not supported for snowpack."), ContentTypeToString(type)));
    }
}

void Snowpack::UpdateContent(double value, ContentType type) {
    switch (type) {
        case ContentType::Water:
            _water->UpdateContent(value);
            break;
        case ContentType::Snow:
            _snow->UpdateContent(value);
            break;
        default:
            throw InvalidArgument(
                wxString::Format(_("The content type '%s' is not supported for snowpack."), ContentTypeToString(type)));
    }
}

void Snowpack::UpdateContentFromInputs() {
    _snow->AddAmountToStaticContentChange(_snow->SumIncomingFluxes());
    _water->AddAmountToDynamicContentChange(_water->SumIncomingFluxes());
}

void Snowpack::ApplyConstraints(double timeStep) {
    _snow->ApplyConstraints(timeStep);
    _water->ApplyConstraints(timeStep);
}

vecDoublePt Snowpack::GetDynamicContentChanges() {
    vecDoublePt vars;
    for (auto const& var : _water->GetDynamicContentChanges()) {
        vars.push_back(var);
    }
    for (auto const& var : _snow->GetDynamicContentChanges()) {
        vars.push_back(var);
    }

    return vars;
}

double* Snowpack::GetValuePointer(const string& name) {
    if (name == "snow" || name == "snow_content") {
        return _snow->GetContentPointer();
    }

    return nullptr;
}

bool Snowpack::HasSnow() {
    return _snow->IsNotEmpty();
}
