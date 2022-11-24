#include "Splitter.h"

#include "HydroUnit.h"
#include "SplitterMultiFluxes.h"
#include "SplitterRain.h"
#include "SplitterSnowRain.h"

Splitter::Splitter() {}

Splitter* Splitter::Factory(const SplitterSettings& splitterSettings) {
    if (splitterSettings.type == "snow_rain") {
        auto splitter = new SplitterSnowRain();
        splitter->AssignParameters(splitterSettings);
        return splitter;
    } else if (splitterSettings.type == "rain") {
        return new SplitterRain();
    } else if (splitterSettings.type == "multi_fluxes") {
        return new SplitterMultiFluxes();
    } else {
        wxLogError(_("Splitter type '%s' not recognized."), splitterSettings.type);
    }

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

    throw MissingParameter(wxString::Format(_("The parameter '%s' could not be found."), name));
}
