#include "ProcessOutflowSnowHoldingPrevah.h"

#include <algorithm>
#include <cmath>

#include "Brick.h"
#include "FluxToBrickInstantaneous.h"
#include "ProcessMelt.h"
#include "Snowpack.h"
#include "TimeMachine.h"
#include "WaterContainer.h"

ProcessOutflowSnowHoldingPrevah::ProcessOutflowSnowHoldingPrevah(WaterContainer* container)
    : ProcessOutflowSnowHolding(container),
      _temperature(nullptr),
      _meltingTemperature(nullptr) {}

void ProcessOutflowSnowHoldingPrevah::RegisterProcessSettings(SettingsModel* modelSettings) {
    ProcessOutflowSnowHolding::RegisterProcessSettings(modelSettings);
    modelSettings->AddProcessParameter("melting_temperature", 0.0f);
    modelSettings->AddProcessParameter("liquid_release_exponent", 0.0f);
    modelSettings->AddProcessForcing("temperature");
}

bool ProcessOutflowSnowHoldingPrevah::IsValid() const {
    if (!ProcessOutflowSnowHolding::IsValid()) {
        return false;
    }
    if (_meltingTemperature == nullptr) {
        LogError("PREVAH snow holding process: missing the 'melting_temperature' parameter.");
        return false;
    }
    if (_liquidReleaseExponent == nullptr) {
        LogError("PREVAH snow holding process: missing the 'liquid_release_exponent' parameter.");
        return false;
    }
    if (_temperature == nullptr) {
        LogError("PREVAH snow holding process requires temperature forcing.");
        return false;
    }

    return true;
}

void ProcessOutflowSnowHoldingPrevah::SetParameters(const ProcessSettings& processSettings) {
    ProcessOutflowSnowHolding::SetParameters(processSettings);
    _meltingTemperature = GetParameterValuePointer(processSettings, "melting_temperature");
    _liquidReleaseExponent = GetParameterValuePointer(processSettings, "liquid_release_exponent");
}

void ProcessOutflowSnowHoldingPrevah::AttachForcing(Forcing* forcing) {
    if (forcing->GetType() == VariableType::Temperature) {
        _temperature = forcing;
    } else {
        throw ModelConfigError("PREVAH snow holding: forcing must be of type Temperature.");
    }
}

ProcessMelt* ProcessOutflowSnowHoldingPrevah::FindSiblingMeltProcess() const {
    Brick* brick = _container->GetParentBrick();
    if (brick == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        auto melt = dynamic_cast<ProcessMelt*>(brick->GetProcess(i));
        if (melt != nullptr) {
            return melt;
        }
    }

    return nullptr;
}

double ProcessOutflowSnowHoldingPrevah::GetMeltThisStep() const {
    // The melt process runs before this release (declaration order sublimation, melt,
    // meltwater, ...) and delivers its melt into the snowpack water container through an
    // instantaneous flux, so its committed real amount is the melt M of this step.
    if (_meltProcess == nullptr || _meltProcess->GetOutputFluxCount() == 0) {
        return 0.0;
    }
    auto* flux = dynamic_cast<FluxToBrickInstantaneous*>(_meltProcess->GetOutputFlux(0));
    if (flux == nullptr) {
        return 0.0;
    }

    return std::max(0.0, flux->GetRealAmount());
}

const vecDouble& ProcessOutflowSnowHoldingPrevah::GetRates() {
    auto snowpack = dynamic_cast<Snowpack*>(_container->GetParentBrick());
    if (snowpack == nullptr) {
        return StoreRates({0});
    }

    double content = _container->GetContentWithChanges();  // C = L + M on a melt day
    double holding;
    if (_temperature->GetValue() > *_meltingTemperature) {
        // PREVAH ablation branch (mxp_snow.f90 lines 342-361). Base behaviour: the
        // retention limit is CWH * snow_liquid, so the store drains to whc x liquid,
        // releasing ~(1 - whc) of the liquid every melt day.
        holding = (*_waterHoldingCapacity) * content;

        // CEXLIQ graded partition of the fresh melt M: a fraction atsliq passes straight
        // through before the collapse, which shifts the retention limit to
        // whc * (C - atsliq * M). atsliq = clamp((L / (whc * solid))^CEXLIQ, 0, 1), with
        // L = C - M the liquid before the melt and solid the snow after it.
        if (*_liquidReleaseExponent > 0) {
            if (_meltProcess == nullptr) {
                _meltProcess = FindSiblingMeltProcess();
            }
            double melt = GetMeltThisStep();
            if (melt > 0) {
                double liquidBefore = std::max(0.0, content - melt);
                double solid = snowpack->GetSnowContainer()->GetContentWithChanges();
                double sliqMax = (*_waterHoldingCapacity) * solid;
                if (sliqMax > 0) {
                    double atsliq = std::pow(liquidBefore / sliqMax, static_cast<double>(*_liquidReleaseExponent));
                    atsliq = std::clamp(atsliq, 0.0, 1.0);
                    holding = (*_waterHoldingCapacity) * (content - atsliq * melt);
                }
            }
        }
    } else {
        double swe = snowpack->GetSnowContainer()->GetContentWithChanges();
        holding = (*_waterHoldingCapacity) * swe;
    }

    double excess = content - holding;
    if (excess <= 0) {
        return StoreRates({0});
    }

    // Absolute amount over the step -> rate (see ProcessOutflowSnowHolding::GetRates).
    double timeStep = (_timeMachine != nullptr) ? *_timeMachine->GetTimeStepPointer() : 1.0;

    return StoreRates({excess / timeStep});
}
