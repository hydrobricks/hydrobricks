#include "ProcessETSocont.h"
#include "Brick.h"

ProcessETSocont::ProcessETSocont(Brick* brick)
    : ProcessET(brick),
      m_pet(nullptr),
      m_stock(nullptr),
      m_stockMax(0.0),
      m_exponent(0.5)
{}

bool ProcessETSocont::IsOk() {
    return ProcessET::IsOk();
}

void ProcessETSocont::AssignParameters(const ProcessSettings &processSettings) {
    wxFAIL;
}

void ProcessETSocont::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == PET) {
        m_pet = forcing;
    } else {
        throw InvalidArgument("Forcing must be of type PET");
    }
}

vecDouble ProcessETSocont::GetChangeRates() {
    return {m_pet->GetValue() * pow(*m_stock / m_stockMax, m_exponent)};
}
