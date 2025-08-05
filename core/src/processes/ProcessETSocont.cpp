#include "ProcessETSocont.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessETSocont::ProcessETSocont(WaterContainer* container)
    : ProcessET(container),
      _pet(nullptr),
      _exponent(0.5) {}

void ProcessETSocont::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessForcing("pet");
}

bool ProcessETSocont::IsOk() {
    return ProcessET::IsOk();
}

void ProcessETSocont::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == PET) {
        _pet = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type PET");
    }
}

vecDouble ProcessETSocont::GetRates() {
    wxASSERT(_container->HasMaximumCapacity());
    return {_pet->GetValue() * pow(_container->GetTargetFillingRatio(), _exponent)};
}
