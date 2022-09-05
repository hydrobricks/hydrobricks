#include "ProcessInfiltration.h"
#include "Brick.h"
#include "WaterContainer.h"

ProcessInfiltration::ProcessInfiltration(WaterContainer* container)
    : Process(container),
      m_targetBrick(nullptr)
{}

bool ProcessInfiltration::IsOk() {
    if (m_outputs.size() != 1) {
        wxLogError(_("An infiltration process should have a single output."));
        return false;
    }
    if (m_targetBrick == nullptr) {
        wxLogError(_("An infiltration process must be linked to a target brick."));
        return false;
    }

    return true;
}

int ProcessInfiltration::GetConnectionsNb() {
    return 1;
}

double* ProcessInfiltration::GetValuePointer(const wxString& name) {
    if (name.IsSameAs("output")) {
        return m_outputs[0]->GetAmountPointer();
    }

    return nullptr;
}

double ProcessInfiltration::GetTargetStock() {
    return m_targetBrick->GetWaterContainer()->GetContentWithChanges();
}

double ProcessInfiltration::GetTargetCapacity() {
    return m_targetBrick->GetWaterContainer()->GetMaximumCapacity();
}

double ProcessInfiltration::GetTargetFillingRatio() {
    return m_targetBrick->GetWaterContainer()->GetTargetFillingRatio();
}