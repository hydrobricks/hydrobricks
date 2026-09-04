#include "ProcessETOpenWaterPrevah.h"

#include <algorithm>

#include "HydroUnit.h"
#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessETOpenWaterPrevah::ProcessETOpenWaterPrevah(WaterContainer* container)
    : ProcessETOpenWater(container),
      _albedoLand(nullptr) {}

void ProcessETOpenWaterPrevah::RegisterProcessSettings(SettingsModel* modelSettings) {
    ProcessETOpenWater::RegisterProcessSettings(modelSettings);
    modelSettings->AddProcessParameter("albedo_land", 0.2f);
    modelSettings->AddProcessParameter("et_factor", 1.0f);
}

bool ProcessETOpenWaterPrevah::IsValid() const {
    if (!ProcessETOpenWater::IsValid()) {
        return false;
    }
    if (_hydroUnit == nullptr) {
        LogError("PREVAH open-water ET process requires a hydro-unit context (hydro-unit bricks only).");
        return false;
    }
    if (_albedoLand == nullptr) {
        LogError("PREVAH open-water ET process: missing the 'albedo_land' parameter.");
        return false;
    }
    if (*_albedoLand < 0 || *_albedoLand >= 1) {
        LogError("PREVAH open-water ET process: the 'albedo_land' parameter must be in [0, 1).");
        return false;
    }
    if (_etFactor == nullptr) {
        LogError("PREVAH open-water ET process: missing the 'et_factor' parameter.");
        return false;
    }
    if (*_etFactor < 0) {
        LogError("PREVAH open-water ET process: the 'et_factor' parameter must be >= 0.");
        return false;
    }

    return true;
}

void ProcessETOpenWaterPrevah::SetParameters(const ProcessSettings& processSettings) {
    Process::SetParameters(processSettings);
    _albedoLand = GetParameterValuePointer(processSettings, "albedo_land");
    _etFactor = GetParameterValuePointer(processSettings, "et_factor");
}

const vecDouble& ProcessETOpenWaterPrevah::GetRates() {
    // PREVAH snow-albedo reduction of the potential rate (see ProcessETPrevah), scaled by
    // the et_factor (PREVAH evaporates the interception store at et_pot * veg_cov, with a
    // monthly vegetation-cover fraction), then the open-water cap at the available content
    // over the time step.
    double albedo = _hydroUnit->GetSnowAlbedo(*_albedoLand);
    double rate = *_etFactor * (1.0 - albedo) / 0.8 * _pet->GetValue();

    if (_timeMachine != nullptr) {
        double timeStepInDays = *_timeMachine->GetTimeStepPointer();
        if (timeStepInDays > 0) {
            double maxRate = _container->GetContentWithChanges() / timeStepInDays;
            rate = std::min(rate, std::max(0.0, maxRate));
        }
    }

    return StoreRates({rate});
}
