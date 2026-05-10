#include "Splitter.h"

#include <functional>
#include <unordered_map>
#include <vector>

#include "HydroUnit.h"
#include "SplitterMultiFluxes.h"
#include "SplitterRain.h"
#include "SplitterSnowRain.h"

Splitter::Splitter() {}

namespace {

using SplitterFactory = std::function<std::unique_ptr<Splitter>(const SplitterSettings&)>;

// clang-format off
const std::unordered_map<string, SplitterFactory>& GetSplitterRegistry() {
    static const std::unordered_map<string, SplitterFactory> registry = {

        {"snow_rain", [](const SplitterSettings& s) {
            auto splitter = std::make_unique<SplitterSnowRain>();
            splitter->SetParameters(s);
            return splitter;
        }},

        {"rain", [](const SplitterSettings&) {
            return std::make_unique<SplitterRain>();
        }},

        {"multi_fluxes", [](const SplitterSettings&) {
            return std::make_unique<SplitterMultiFluxes>();
        }},

    };
    return registry;
}
// clang-format on

string GetValidSplitterTypes() {
    const auto& registry = GetSplitterRegistry();
    string suggestions = "Valid splitter types: ";
    bool first = true;
    for (const auto& [key, factory] : registry) {
        if (!first) suggestions += ", ";
        suggestions += key;
        first = false;
    }
    return suggestions;
}

}  // namespace

std::unique_ptr<Splitter> Splitter::Factory(const SplitterSettings& splitterSettings) {
    const auto& registry = GetSplitterRegistry();
    auto it = registry.find(splitterSettings.type);
    if (it != registry.end()) {
        return it->second(splitterSettings);
    }
    LogError("Splitter type '{}' not recognized. {}", splitterSettings.type, GetValidSplitterTypes());
    return nullptr;
}

const float* Splitter::GetParameterValuePointer(const SplitterSettings& splitterSettings, const string& name) {
    for (const auto& parameter : splitterSettings.parameters) {
        if (parameter.GetName() == name) {
            assert(parameter.GetValuePointer());
            return parameter.GetValuePointer();
        }
    }

    throw ModelConfigError(std::format("The parameter '{}' could not be found.", name));
}

void Splitter::Validate() const {
    if (!IsValid()) {
        throw ModelConfigError("Splitter validation failed. Check that all required properties are correctly defined.");
    }
}
