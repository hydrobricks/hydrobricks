#include "Splitter.h"

#include <vector>

#include "HydroUnit.h"
#include "SplitterMultiFluxes.h"
#include "SplitterRain.h"
#include "SplitterSnowRain.h"

Splitter::Splitter() {}

static string GetValidSplitterTypes() {
    static const vector<string> validTypes = {
        "snow_rain",
        "rain",
        "multi_fluxes"
    };

    string suggestions = "Valid splitter types: ";
    for (size_t i = 0; i < validTypes.size(); ++i) {
        suggestions += validTypes[i];
        if (i < validTypes.size() - 1) {
            suggestions += ", ";
        }
    }
    return suggestions;
}

Splitter* Splitter::Factory(const SplitterSettings& splitterSettings) {
    if (splitterSettings.type == "snow_rain") {
        auto splitter = new SplitterSnowRain();
        splitter->SetParameters(splitterSettings);
        return splitter;
    }
    if (splitterSettings.type == "rain") {
        return new SplitterRain();
    }
    if (splitterSettings.type == "multi_fluxes") {
        return new SplitterMultiFluxes();
    }
    wxLogError(_("Splitter type '%s' not recognized. %s"), splitterSettings.type, GetValidSplitterTypes());

    return nullptr;
}

float* Splitter::GetParameterValuePointer(const SplitterSettings& splitterSettings, const string& name) {
    for (auto parameter : splitterSettings.parameters) {
        if (parameter->GetName() == name) {
            wxASSERT(parameter->GetValuePointer());
            parameter->SetAsLinked();
            return parameter->GetValuePointer();
        }
    }

    throw ModelConfigError(wxString::Format(_("The parameter '%s' could not be found."), name));
}
