#include "ProcessRefreezeDegreeDay.h"

#include "Brick.h"
#include "ProcessMeltDegreeDay.h"
#include "WaterContainer.h"

ProcessRefreezeDegreeDay::ProcessRefreezeDegreeDay(WaterContainer* container)
    : ProcessTransform(container),
      _temperature(nullptr),
      _refreezingFactor(nullptr),
      _meltProcess(nullptr) {}

void ProcessRefreezeDegreeDay::RegisterProcessSettings(SettingsModel* modelSettings) {
    modelSettings->AddProcessParameter("refreezing_factor", 0.05f);
    modelSettings->AddProcessForcing("temperature");
}

bool ProcessRefreezeDegreeDay::IsValid() const {
    if (!ProcessTransform::IsValid()) {
        return false;
    }
    if (_temperature == nullptr) {
        LogError("Degree-day refreeze process requires temperature forcing.");
        return false;
    }
    if (_refreezingFactor == nullptr) {
        LogError("Degree-day refreeze process: missing the 'refreezing_factor' parameter.");
        return false;
    }
    if (FindSiblingMeltProcess() == nullptr) {
        LogError("The refreeze:degree_day process requires a melt:degree_day process on the same brick.");
        return false;
    }

    return true;
}

void ProcessRefreezeDegreeDay::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _refreezingFactor = GetParameterValuePointer(processSettings, "refreezing_factor");
}

void ProcessRefreezeDegreeDay::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::Temperature) {
        _temperature = forcing;
    } else {
        throw ModelConfigError("Degree-day refreeze: forcing must be of type Temperature.");
    }
}

ProcessMeltDegreeDay* ProcessRefreezeDegreeDay::FindSiblingMeltProcess() const {
    Brick* brick = _container->GetParentBrick();
    if (brick == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        auto melt = dynamic_cast<ProcessMeltDegreeDay*>(brick->GetProcess(i));
        if (melt != nullptr) {
            return melt;
        }
    }

    return nullptr;
}

vecDouble ProcessRefreezeDegreeDay::GetRates() {
    if (_meltProcess == nullptr) {
        _meltProcess = FindSiblingMeltProcess();
        if (_meltProcess == nullptr) {
            return {0};
        }
    }

    double meltingTemperature = _meltProcess->GetMeltingTemperature();
    if (_temperature->GetValue() >= meltingTemperature) {
        return {0};
    }

    double refreeze = (*_refreezingFactor) * _meltProcess->GetDegreeDayFactor() *
                      (meltingTemperature - _temperature->GetValue());

    return {refreeze};
}
