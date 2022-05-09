#include "ProcessETSocont.h"
#include "Brick.h"

ProcessETSocont::ProcessETSocont(Brick* brick)
    : Process(brick),
      m_pet(nullptr),
      m_stock(nullptr),
      m_stockMax(0.0),
      m_exponent(0.5)
{}

bool ProcessETSocont::IsOk() {
    if (m_outputs.size() != 1) {
        wxLogError(_("The linear storage should have a single output."));
        return false;
    }

    return true;
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

double ProcessETSocont::GetWaterExtraction() {
    return m_pet->GetValue() * pow(*m_stock / m_stockMax, m_exponent);
}
