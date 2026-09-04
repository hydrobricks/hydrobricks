#include "Snowpack.h"

#include <cmath>

Snowpack::Snowpack()
    : SurfaceComponent(),
      _snow(std::make_unique<SnowContainer>(this)) {
    _category = BrickCategory::Snowpack;
}

void Snowpack::Reset() {
    _water->Reset();
    _snow->Reset();
    _snowAge = 0;
    _snowfallInput = 0;
}

void Snowpack::SaveAsInitialState() {
    _water->SaveAsInitialState();
    _snow->SaveAsInitialState();
}

void Snowpack::SetParameters(const BrickSettings& brickSettings) {
    Brick::SetParameters(brickSettings);
}

void Snowpack::AttachFluxIn(Flux* flux) {
    assert(flux);
    if (flux->GetType() == ContentType::Snow) {
        _snow->AttachFluxIn(flux);
    } else if (flux->GetType() == ContentType::Water) {
        _water->AttachFluxIn(flux);
    } else {
        throw ShouldNotHappen(
            std::format("Snowpack::AttachFluxIn - Unexpected flux type: {}", static_cast<int>(flux->GetType())));
    }
}

bool Snowpack::IsValid(bool checkProcesses) const {
    if (!_snow->IsValid()) {
        return false;
    }
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
    // We do not validate the water container here because it may not be used in all configurations.
    return true;
}

WaterContainer* Snowpack::GetSnowContainer() const {
    return _snow.get();
}

void Snowpack::Finalize() {
    _snow->Finalize();
    _water->Finalize();

    // Finalize processes that maintain explicit internal state across the time step
    // (e.g. the dynamic snow density tracked by the Frey & Holzmann redistribution).
    for (int i = 0; i < GetProcessCount(); ++i) {
        GetProcess(i)->Finalize();
    }
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
            throw ModelConfigError(
                std::format("The content type '{}' is not supported for snowpack.", ContentTypeToString(type)));
    }
}

double Snowpack::GetContent(ContentType type) const {
    switch (type) {
        case ContentType::Water:
            return _water->GetContentWithoutChanges();
        case ContentType::Snow:
            return _snow->GetContentWithoutChanges();
        default:
            throw ModelConfigError(
                std::format("The content type '{}' is not supported for snowpack.", ContentTypeToString(type)));
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
            throw ModelConfigError(
                std::format("The content type '{}' is not supported for snowpack.", ContentTypeToString(type)));
    }
}

void Snowpack::UpdateContentFromInputs() {
    // Update the snow-surface age at the start of the step, before any process reads the
    // albedo, so all consumers (the snow evaporation and the albedo-reduced ET) see a
    // consistent age (PREVAH sxp_core.f08 / mxp_snow.f90): reset to 0 on a fresh
    // snowfall, otherwise increment by one step while the snow persists (reset when the
    // snowpack is empty). The content here is still the previous step's committed value.
    _snowfallInput = _snow->SumIncomingFluxes();
    if (_snowfallInput >= 0.01) {
        _snowAge = 0;
    } else if (_snow->GetContentWithoutChanges() > 0.01) {
        _snowAge += 1;
    } else {
        _snowAge = 0;
    }

    _snow->AddAmountToStaticContentChange(_snowfallInput);
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

double* Snowpack::GetValuePointer(std::string_view name) {
    if (name == "snow" || name == "snow_content") {
        return _snow->GetContentPointer();
    }

    return nullptr;
}

bool Snowpack::HasSnow() const {
    return _snow->IsNotEmpty();
}

double Snowpack::GetSnowAlbedo() const {
    return 0.4 + 0.45 * std::exp(-0.15 * _snowAge);
}
