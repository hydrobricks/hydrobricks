#include "Splitter.h"

#include "HydroUnit.h"
#include "SplitterMultiFluxes.h"
#include "SplitterSnowRain.h"

Splitter::Splitter(HydroUnit *hydroUnit)
{
    if (hydroUnit) {
        hydroUnit->AddSplitter(this);
    }
}

Splitter* Splitter::Factory(const SplitterSettings &splitterSettings, HydroUnit* unit) {
    if (splitterSettings.type.IsSameAs("SnowRain")) {
        auto splitter = new SplitterSnowRain(unit);
        splitter->AssignParameters(splitterSettings);
        return splitter;
    } else if (splitterSettings.type.IsSameAs("MultiFluxes")) {
        return new SplitterMultiFluxes(unit);
    } else {
        wxLogError(_("Splitter type '%s' not recognized."), splitterSettings.type);
    }

    return nullptr;
}

float* Splitter::GetParameterValuePointer(const SplitterSettings &splitterSettings, const wxString &name) {
    for (auto parameter: splitterSettings.parameters) {
        if (parameter->GetName().IsSameAs(name, false)) {
            wxASSERT(parameter->GetValuePointer());
            parameter->SetAsLinked();
            return parameter->GetValuePointer();
        }
    }

    throw MissingParameter(wxString::Format(_("The parameter '%s' could not be found."), name));
}