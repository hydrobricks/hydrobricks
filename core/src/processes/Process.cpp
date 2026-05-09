#include "Process.h"

#include <functional>
#include <unordered_map>
#include <vector>

#include "Brick.h"
#include "Glacier.h"
#include "HydroUnit.h"
#include "ProcessETSocont.h"
#include "ProcessInfiltrationSocont.h"
#include "ProcessLateralSnowSlide.h"
#include "ProcessMeltDegreeDay.h"
#include "ProcessMeltDegreeDayAspect.h"
#include "ProcessMeltTemperatureIndex.h"
#include "ProcessOutflowDirect.h"
#include "ProcessOutflowLinear.h"
#include "ProcessOutflowOverflow.h"
#include "ProcessOutflowPercolation.h"
#include "ProcessOutflowRestDirect.h"
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
        {"percolation", "outflow:percolation"},
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
            [](Brick* b) { return std::make_unique<ProcessOutflowLinear>(b->GetWaterContainer()); },
            &ProcessOutflowLinear::RegisterProcessParametersAndForcing
        }},

        {"outflow:percolation", {
            [](Brick* b) { return std::make_unique<ProcessOutflowPercolation>(b->GetWaterContainer()); },
            &ProcessOutflowPercolation::RegisterProcessParametersAndForcing
        }},

        {"outflow:direct", {
            [](Brick* b) { return std::make_unique<ProcessOutflowDirect>(b->GetWaterContainer()); },
            &ProcessOutflowDirect::RegisterProcessParametersAndForcing
        }},

        {"outflow:rest_direct", {
            [](Brick* b) { return std::make_unique<ProcessOutflowRestDirect>(b->GetWaterContainer()); },
            &ProcessOutflowRestDirect::RegisterProcessParametersAndForcing
        }},

        {"outflow:overflow", {
            [](Brick* b) { return std::make_unique<ProcessOutflowOverflow>(b->GetWaterContainer()); },
            &ProcessOutflowOverflow::RegisterProcessParametersAndForcing
        }},

        {"runoff:socont", {
            [](Brick* b) { return std::make_unique<ProcessRunoffSocont>(b->GetWaterContainer()); },
            &ProcessRunoffSocont::RegisterProcessParametersAndForcing
        }},

        {"infiltration:socont", {
            [](Brick* b) { return std::make_unique<ProcessInfiltrationSocont>(b->GetWaterContainer()); },
            &ProcessInfiltrationSocont::RegisterProcessParametersAndForcing
        }},

        {"et:socont", {
            [](Brick* b) { return std::make_unique<ProcessETSocont>(b->GetWaterContainer()); },
            &ProcessETSocont::RegisterProcessParametersAndForcing
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
            &ProcessMeltDegreeDay::RegisterProcessParametersAndForcing
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
            &ProcessMeltDegreeDayAspect::RegisterProcessParametersAndForcing
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
            &ProcessMeltTemperatureIndex::RegisterProcessParametersAndForcing
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
            &ProcessTransformSnowToIceConstant::RegisterProcessParametersAndForcing
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
            &ProcessTransformSnowToIceSwat::RegisterProcessParametersAndForcing
        }},

        {"transport:snow_slide", {
            [](Brick* b) -> std::unique_ptr<Process> {
                if (b->GetCategory() == BrickCategory::Snowpack) {
                    return std::make_unique<ProcessLateralSnowSlide>(dynamic_cast<Snowpack*>(b)->GetSnowContainer());
                }
                throw ModelConfigError(
                    std::format("Trying to apply transport processes to unsupported brick: {}", b->GetName()));
            },
            &ProcessLateralSnowSlide::RegisterProcessParametersAndForcing
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

bool Process::RegisterParametersAndForcing(SettingsModel* modelSettings, const string& processType) {
    const auto& registry = GetProcessRegistry();
    auto it = registry.find(ResolveAlias(processType));
    if (it != registry.end()) {
        it->second.registerFn(modelSettings);
        return true;
    }
    throw ModelConfigError(std::format("Process type '{}' not recognized (RegisterParametersAndForcing). {}",
                                       processType, GetValidProcessTypes()));
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

vecDouble Process::GetChangeRates() {
    if (LessThanOrEqual(_container->GetContentWithChanges(), 0, PRECISION)) {
        vecDouble res(GetConnectionCount());
        std::fill(res.begin(), res.end(), 0);
        return res;
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
