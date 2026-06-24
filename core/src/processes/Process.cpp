#include "Process.h"

#include <functional>
#include <unordered_map>
#include <vector>

#include "Brick.h"
#include "Glacier.h"
#include "HydroUnit.h"
#include "ProcessCapillaryHBV.h"
#include "ProcessETExponential.h"
#include "ProcessETGR4J.h"
#include "ProcessETHBV.h"
#include "ProcessETLinear.h"
#include "ProcessETOpenWater.h"
#include "ProcessETPowerLaw.h"
#include "ProcessETSocont.h"
#include "ProcessInfiltrationGR4J.h"
#include "ProcessInfiltrationHBV.h"
#include "ProcessInfiltrationSocont.h"
#include "ProcessInterceptionGR4J.h"
#include "ProcessLateralSnowSlide.h"
#include "ProcessMeltCemaNeige.h"
#include "ProcessMeltDegreeDay.h"
#include "ProcessMeltDegreeDayAspect.h"
#include "ProcessMeltTemperatureIndex.h"
#include "ProcessOutflowDirect.h"
#include "ProcessOutflowLinear.h"
#include "ProcessOutflowOverflow.h"
#include "ProcessOutflowRest.h"
#include "ProcessOutflowSnowHolding.h"
#include "ProcessOutflowThreshold.h"
#include "ProcessPercolationConstant.h"
#include "ProcessPercolationGR4J.h"
#include "ProcessProductionGR4J.h"
#include "ProcessRefreezeDegreeDay.h"
#include "ProcessRoutingGR4J.h"
#include "ProcessRoutingGR6J.h"
#include "ProcessRoutingHBV.h"
#include "ProcessRunoffHBV.h"
#include "ProcessRunoffSocont.h"
#include "ProcessTransformSnowToIceConstant.h"
#include "ProcessTransformSnowToIceSwat.h"
#include "Snowpack.h"
#include "WaterContainer.h"

Process::Process(WaterContainer* container)
    : _container(container) {}

namespace {

struct ProcessEntry {
    std::function<std::unique_ptr<Process>(Brick*)> create;
    std::function<void(SettingsModel*)> registerFn;
};

// Maps deprecated/short aliases to their canonical process type name.
const std::unordered_map<string, string>& GetProcessAliases() {
    static const std::unordered_map<string, string> aliases = {
        {"outflow:percolation", "percolation:constant"},
        {"overflow", "outflow:overflow"},
        {"transform:snow_ice_constant", "transformation:snow_ice_constant"},
        {"transform:snow_ice_swat", "transformation:snow_ice_swat"},
    };
    return aliases;
}

const string& ResolveAlias(const string& type) {
    const auto& aliases = GetProcessAliases();
    auto it = aliases.find(type);
    return it != aliases.end() ? it->second : type;
}

// clang-format off
const std::unordered_map<string, ProcessEntry>& GetProcessRegistry() {
    static const std::unordered_map<string, ProcessEntry> registry = {

        {"outflow:linear", {
            [](Brick* b) {
                return std::make_unique<ProcessOutflowLinear>(b->GetWaterContainer());
            },
            &ProcessOutflowLinear::RegisterProcessSettings
        }},

        {"percolation:constant", {
            [](Brick* b) {
                return std::make_unique<ProcessPercolationConstant>(b->GetWaterContainer());
            },
            &ProcessPercolationConstant::RegisterProcessSettings
        }},

        {"percolation:gr4j", {
            [](Brick* b) {
                return std::make_unique<ProcessPercolationGR4J>(b->GetWaterContainer());
            },
            &ProcessPercolationGR4J::RegisterProcessSettings
        }},

        {"outflow:direct", {
            [](Brick* b) {
                return std::make_unique<ProcessOutflowDirect>(b->GetWaterContainer());
            },
            &ProcessOutflowDirect::RegisterProcessSettings
        }},

        {"outflow:rest", {
            [](Brick* b) {
                return std::make_unique<ProcessOutflowRest>(b->GetWaterContainer());
            },
            &ProcessOutflowRest::RegisterProcessSettings
        }},

        {"outflow:overflow", {
            [](Brick* b) {
                return std::make_unique<ProcessOutflowOverflow>(b->GetWaterContainer());
            },
            &ProcessOutflowOverflow::RegisterProcessSettings
        }},

        {"runoff:socont", {
            [](Brick* b) {
                return std::make_unique<ProcessRunoffSocont>(b->GetWaterContainer());
            },
            &ProcessRunoffSocont::RegisterProcessSettings
        }},

        {"infiltration:socont", {
            [](Brick* b) {
                return std::make_unique<ProcessInfiltrationSocont>(b->GetWaterContainer());
            },
            &ProcessInfiltrationSocont::RegisterProcessSettings
        }},

        {"infiltration:gr4j", {
            [](Brick* b) {
                return std::make_unique<ProcessInfiltrationGR4J>(b->GetWaterContainer());
            },
            &ProcessInfiltrationGR4J::RegisterProcessSettings
        }},

        {"infiltration:hbv", {
            [](Brick* b) {
                return std::make_unique<ProcessInfiltrationHBV>(b->GetWaterContainer());
            },
            &ProcessInfiltrationHBV::RegisterProcessSettings
        }},

        {"runoff:hbv", {
            [](Brick* b) {
                return std::make_unique<ProcessRunoffHBV>(b->GetWaterContainer());
            },
            &ProcessRunoffHBV::RegisterProcessSettings
        }},

        {"capillary:hbv", {
            [](Brick* b) {
                return std::make_unique<ProcessCapillaryHBV>(b->GetWaterContainer());
            },
            &ProcessCapillaryHBV::RegisterProcessSettings
        }},

        {"routing:hbv", {
            [](Brick* b) {
                return std::make_unique<ProcessRoutingHBV>(b->GetWaterContainer());
            },
            &ProcessRoutingHBV::RegisterProcessSettings
        }},

        {"outflow:snow_holding", {
            [](Brick* b) {
                return std::make_unique<ProcessOutflowSnowHolding>(b->GetWaterContainer());
            },
            &ProcessOutflowSnowHolding::RegisterProcessSettings
        }},

        {"outflow:threshold", {
            [](Brick* b) {
                return std::make_unique<ProcessOutflowThreshold>(b->GetWaterContainer());
            },
            &ProcessOutflowThreshold::RegisterProcessSettings
        }},

        {"refreeze:degree_day", {
            [](Brick* b) {
                return std::make_unique<ProcessRefreezeDegreeDay>(b->GetWaterContainer());
            },
            &ProcessRefreezeDegreeDay::RegisterProcessSettings
        }},

        {"production:gr4j", {
            [](Brick* b) {
                return std::make_unique<ProcessProductionGR4J>(b->GetWaterContainer());
            },
            &ProcessProductionGR4J::RegisterProcessSettings
        }},

        {"et:socont", {
            [](Brick* b) {
                return std::make_unique<ProcessETSocont>(b->GetWaterContainer());
            },
            &ProcessETSocont::RegisterProcessSettings
        }},

        {"et:gr4j", {
            [](Brick* b) {
                return std::make_unique<ProcessETGR4J>(b->GetWaterContainer());
            },
            &ProcessETGR4J::RegisterProcessSettings
        }},

        {"et:hbv", {
            [](Brick* b) {
                return std::make_unique<ProcessETHBV>(b->GetWaterContainer());
            },
            &ProcessETHBV::RegisterProcessSettings
        }},

        {"et:open_water", {
            [](Brick* b) {
                return std::make_unique<ProcessETOpenWater>(b->GetWaterContainer());
            },
            &ProcessETOpenWater::RegisterProcessSettings
        }},

        {"et:linear", {
            [](Brick* b) {
                return std::make_unique<ProcessETLinear>(b->GetWaterContainer());
            },
            &ProcessETLinear::RegisterProcessSettings
        }},

        {"et:power_law", {
            [](Brick* b) {
                return std::make_unique<ProcessETPowerLaw>(b->GetWaterContainer());
            },
            &ProcessETPowerLaw::RegisterProcessSettings
        }},

        {"et:exponential", {
            [](Brick* b) {
                return std::make_unique<ProcessETExponential>(b->GetWaterContainer());
            },
            &ProcessETExponential::RegisterProcessSettings
        }},

        {"melt:degree_day", {
            [](Brick* b) -> std::unique_ptr<Process> {
                if (b->GetCategory() == BrickCategory::Snowpack) {
                    return std::make_unique<ProcessMeltDegreeDay>(dynamic_cast<Snowpack*>(b)->GetSnowContainer());
                }
                if (b->GetCategory() == BrickCategory::Glacier) {
                    return std::make_unique<ProcessMeltDegreeDay>(dynamic_cast<Glacier*>(b)->GetIceContainer());
                }
                throw ModelConfigError(
                    std::format("Trying to apply melting processes to unsupported brick: {}", b->GetName()));
            },
            &ProcessMeltDegreeDay::RegisterProcessSettings
        }},

        {"melt:degree_day_aspect", {
            [](Brick* b) -> std::unique_ptr<Process> {
                if (b->GetCategory() == BrickCategory::Snowpack) {
                    return std::make_unique<ProcessMeltDegreeDayAspect>(dynamic_cast<Snowpack*>(b)->GetSnowContainer());
                }
                if (b->GetCategory() == BrickCategory::Glacier) {
                    return std::make_unique<ProcessMeltDegreeDayAspect>(dynamic_cast<Glacier*>(b)->GetIceContainer());
                }
                throw ModelConfigError(
                    std::format("Trying to apply melting processes to unsupported brick: {}", b->GetName()));
            },
            &ProcessMeltDegreeDayAspect::RegisterProcessSettings
        }},

        {"melt:temperature_index", {
            [](Brick* b) -> std::unique_ptr<Process> {
                if (b->GetCategory() == BrickCategory::Snowpack) {
                    return std::make_unique<ProcessMeltTemperatureIndex>(dynamic_cast<Snowpack*>(b)->GetSnowContainer());
                }
                if (b->GetCategory() == BrickCategory::Glacier) {
                    return std::make_unique<ProcessMeltTemperatureIndex>(dynamic_cast<Glacier*>(b)->GetIceContainer());
                }
                throw ModelConfigError(
                    std::format("Trying to apply melting processes to unsupported brick: {}", b->GetName()));
            },
            &ProcessMeltTemperatureIndex::RegisterProcessSettings
        }},

        {"melt:cemaneige", {
            [](Brick* b) -> std::unique_ptr<Process> {
                if (b->GetCategory() == BrickCategory::Snowpack) {
                    return std::make_unique<ProcessMeltCemaNeige>(dynamic_cast<Snowpack*>(b)->GetSnowContainer());
                }
                throw ModelConfigError(
                    std::format("Trying to apply melt:cemaneige to unsupported brick: {}", b->GetName()));
            },
            &ProcessMeltCemaNeige::RegisterProcessSettings
        }},

        {"transformation:snow_ice_constant", {
            [](Brick* b) -> std::unique_ptr<Process> {
                if (b->GetCategory() == BrickCategory::Snowpack) {
                    return std::make_unique<ProcessTransformSnowToIceConstant>(
                        dynamic_cast<Snowpack*>(b)->GetSnowContainer());
                }
                throw ModelConfigError(std::format(
                    "Trying to apply transformation processes to unsupported brick: {}", b->GetName()));
            },
            &ProcessTransformSnowToIceConstant::RegisterProcessSettings
        }},

        {"transformation:snow_ice_swat", {
            [](Brick* b) -> std::unique_ptr<Process> {
                if (b->GetCategory() == BrickCategory::Snowpack) {
                    return std::make_unique<ProcessTransformSnowToIceSwat>(
                        dynamic_cast<Snowpack*>(b)->GetSnowContainer());
                }
                throw ModelConfigError(std::format(
                    "Trying to apply transformation processes to unsupported brick: {}", b->GetName()));
            },
            &ProcessTransformSnowToIceSwat::RegisterProcessSettings
        }},

        {"transport:snow_slide", {
            [](Brick* b) -> std::unique_ptr<Process> {
                if (b->GetCategory() == BrickCategory::Snowpack) {
                    return std::make_unique<ProcessLateralSnowSlide>(dynamic_cast<Snowpack*>(b)->GetSnowContainer());
                }
                throw ModelConfigError(
                    std::format("Trying to apply transport processes to unsupported brick: {}", b->GetName()));
            },
            &ProcessLateralSnowSlide::RegisterProcessSettings
        }},

        {"interception:gr4j", {
            [](Brick* b) {
                return std::make_unique<ProcessInterceptionGR4J>(b->GetWaterContainer());
            },
            &ProcessInterceptionGR4J::RegisterProcessSettings
        }},

        {"routing:gr4j", {
            [](Brick* b) {
                return std::make_unique<ProcessRoutingGR4J>(b->GetWaterContainer());
            },
            &ProcessRoutingGR4J::RegisterProcessSettings
        }},

        {"routing:gr6j", {
            [](Brick* b) {
                return std::make_unique<ProcessRoutingGR6J>(b->GetWaterContainer());
            },
            &ProcessRoutingGR6J::RegisterProcessSettings
        }},

    };
    return registry;
}
// clang-format on

string GetValidProcessTypes() {
    const auto& registry = GetProcessRegistry();
    string suggestions = "Valid process types: ";
    bool first = true;
    for (const auto& [key, entry] : registry) {
        if (!first) suggestions += ", ";
        suggestions += key;
        first = false;
    }
    return suggestions;
}

}  // namespace

std::unique_ptr<Process> Process::Factory(const ProcessSettings& processSettings, Brick* brick) {
    const auto& registry = GetProcessRegistry();
    auto it = registry.find(ResolveAlias(processSettings.type));
    if (it != registry.end()) {
        return it->second.create(brick);
    }
    throw ModelConfigError(
        std::format("Process type '{}' not recognized (Factory). {}", processSettings.type, GetValidProcessTypes()));
}

bool Process::RegisterSettings(SettingsModel* modelSettings, const string& processType) {
    const auto& registry = GetProcessRegistry();
    auto it = registry.find(ResolveAlias(processType));
    if (it != registry.end()) {
        it->second.registerFn(modelSettings);
        return true;
    }
    throw ModelConfigError(
        std::format("Process type '{}' not recognized (RegisterSettings). {}", processType, GetValidProcessTypes()));
}

void Process::Reset() {
    for (const auto& flux : _outputs) {
        flux->Reset();
    }
}

void Process::SetHydroUnitProperties(HydroUnit*, Brick*) {
    // Nothing to do...
}

void Process::SetParameters(const ProcessSettings&) {
    // Nothing to do...
}

bool Process::HasParameter(const ProcessSettings& processSettings, std::string_view name) {
    for (const auto& parameter : processSettings.parameters) {
        if (parameter.GetName() == name) {
            return true;
        }
    }

    return false;
}

const float* Process::GetParameterValuePointer(const ProcessSettings& processSettings, std::string_view name) {
    for (auto& parameter : processSettings.parameters) {
        if (parameter.GetName() == name) {
            assert(parameter.GetValuePointer());
            return parameter.GetValuePointer();
        }
    }

    throw ModelConfigError(std::format("The parameter '{}' could not be found.", name));
}

const vecDouble& Process::GetChangeRates() {
    if (LessThanOrEqual(_container->GetContentWithChanges(), 0, PRECISION)) {
        _changeRates.assign(GetConnectionCount(), 0.0);
        return _changeRates;
    }

    return GetRates();
}

void Process::StoreInOutgoingFlux(double* rate, int index) {
    assert(_outputs.size() > index);
    assert(rate);
    _outputs[index]->LinkChangeRate(rate);
}

void Process::ApplyChange(int connectionIndex, double rate, double timeStepInDays) {
    assert(_outputs.size() > connectionIndex);
    assert(rate >= 0);
    if (rate < 0) {
        LogError("Negative rate ({}) in process {}, connection {}.", rate, GetName(), connectionIndex);
        rate = 0;
    }
    if (GreaterThan(rate, 0, PRECISION)) {
        _outputs[connectionIndex]->UpdateFlux(rate * timeStepInDays);
        _container->SubtractAmountFromDynamicContentChange(rate * timeStepInDays);
    } else {
        _outputs[connectionIndex]->UpdateFlux(0);
    }
}

double* Process::GetValuePointer(std::string_view) {
    return nullptr;
}

double Process::GetSumChangeRatesOtherProcesses() const {
    double sumOtherProcesses = 0;

    for (int i = 0; i < _container->GetParentBrick()->GetProcessCount(); ++i) {
        auto process = _container->GetParentBrick()->GetProcess(i);
        assert(process);
        if (process == this) {
            continue;
        }
        for (int j = 0; j < process->GetOutputFluxCount(); ++j) {
            Flux* flux = process->GetOutputFlux(j);
            assert(flux);
            sumOtherProcesses += *flux->GetAmountPointer();
        }
    }

    return sumOtherProcesses;
}

void Process::Validate() const {
    if (!IsValid()) {
        throw ModelConfigError("Process validation failed. Check that all required properties are correctly defined.");
    }
}
