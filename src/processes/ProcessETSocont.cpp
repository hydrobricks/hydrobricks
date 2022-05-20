#include "ProcessETSocont.h"
#include "Brick.h"

ProcessETSocont::ProcessETSocont(Brick* brick)
    : ProcessET(brick),
      m_pet(nullptr),
      m_exponent(0.5)
{}

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

vecDouble ProcessETSocont::GetChangeRates() {
    wxASSERT(m_brick->GetWaterContainer()->HasMaximumCapacity());
    return {m_pet->GetValue() * pow(m_brick->GetWaterContainer()->GetContentWithChanges() /
                                        m_brick->GetWaterContainer()->GetMaximumCapacity(),
                                    m_exponent)};
}
