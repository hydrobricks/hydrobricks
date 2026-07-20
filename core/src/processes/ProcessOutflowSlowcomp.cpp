#include "ProcessOutflowSlowcomp.h"

#include <algorithm>

#include "Brick.h"
#include "ProcessOutflowLinear.h"
#include "WaterContainer.h"

ProcessOutflowSlowcomp::ProcessOutflowSlowcomp(WaterContainer* container)
    : ProcessOutflow(container),
      _maxContent(nullptr) {}

void ProcessOutflowSlowcomp::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("max_content", 100.0f);
}

bool ProcessOutflowSlowcomp::IsValid() const {
    if (!ProcessOutflow::IsValid()) {
        return false;
    }
    if (_maxContent == nullptr) {
        LogError("SLOWCOMP overflow process: missing the 'max_content' parameter.");
        return false;
    }
    if (FindSiblingBaseflow() == nullptr) {
        LogError("The outflow:slowcomp process requires a linear baseflow (outflow:linear) on the same brick.");
        return false;
    }

    return true;
}

void ProcessOutflowSlowcomp::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _maxContent = GetParameterValuePointer(processSettings, "max_content");
}

ProcessOutflowLinear* ProcessOutflowSlowcomp::FindSiblingBaseflow() const {
    Brick* brick = _container->GetParentBrick();
    if (brick == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        auto baseflow = dynamic_cast<ProcessOutflowLinear*>(brick->GetProcess(i));
        if (baseflow != nullptr) {
            return baseflow;
        }
    }

    return nullptr;
}

const vecDouble& ProcessOutflowSlowcomp::GetRates() {
    if (_baseflow == nullptr) {
        _baseflow = FindSiblingBaseflow();
        if (_baseflow == nullptr) {
            return StoreRates({0});
        }
    }

    // K1 is the sibling baseflow time constant; k1 = 1/K1 is its response factor.
    double k1 = _baseflow->GetResponseFactor();
    double content = _container->GetContentWithChanges();
    double inflow = _container->SumIncomingChangeRates();

    // The store can only absorb inflow up to (max_content - content)/K1 = (max_content
    // - content)*k1 (asymptotic fill); the excess overflows to the slow stores.
    double absorbLimit = (*_maxContent - content) * k1;

    return StoreRates({std::max(0.0, inflow - absorbLimit)});
}
