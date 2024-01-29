#include "ProcessETSocont.h"

#include "Brick.h"
#include "WaterContainer.h"

ProcessETSocont::ProcessETSocont(WaterContainer* container)
    : ProcessET(container),
      m_pet(nullptr),
      m_exponent(0.5) {}

void ProcessETSocont::RegisterProcessParametersAndForcing(SettingsModel* modelSettings) {
    modelSettings->AddProcessForcing("pet");
}

bool ProcessETSocont::IsOk() {
    return ProcessET::IsOk();
}

void ProcessETSocont::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == PET) {
        m_pet = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type PET");
    }
}

vecDouble ProcessETSocont::GetRates() {
    wxASSERT(m_container->HasMaximumCapacity());
    return {m_pet->GetValue() * pow(m_container->GetTargetFillingRatio(), m_exponent)};
}
