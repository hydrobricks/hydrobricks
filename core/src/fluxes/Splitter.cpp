#include "Splitter.h"

#include "HydroUnit.h"
#include "SplitterMultiFluxes.h"
#include "SplitterRain.h"
#include "SplitterSnowRain.h"

Splitter::Splitter() {}

Splitter* Splitter::Factory(const SplitterSettings& splitterSettings) {
    if (splitterSettings.type == "SnowRain") {
        auto splitter = new SplitterSnowRain();
        splitter->AssignParameters(splitterSettings);
        return splitter;
    } else if (splitterSettings.type == "Rain") {
        return new SplitterRain();
    } else if (splitterSettings.type == "MultiFluxes") {
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
