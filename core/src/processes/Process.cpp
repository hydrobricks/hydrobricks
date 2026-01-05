#include "Process.h"

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

Process* Process::Factory(const ProcessSettings& processSettings, Brick* brick) {
    string processType = processSettings.type;

    if (processType == "outflow:linear") {
        return new ProcessOutflowLinear(brick->GetWaterContainer());
    }
    if (processType == "outflow:percolation" || processType == "percolation") {
        return new ProcessOutflowPercolation(brick->GetWaterContainer());
    }
    if (processType == "outflow:direct") {
        return new ProcessOutflowDirect(brick->GetWaterContainer());
    }
    if (processType == "outflow:rest_direct") {
        return new ProcessOutflowRestDirect(brick->GetWaterContainer());
    }
    if (processType == "outflow:overflow" || processType == "overflow") {
        return new ProcessOutflowOverflow(brick->GetWaterContainer());
    }
    if (processType == "transformation:snow_ice_constant" || processType == "transform:snow_ice_constant") {
        if (brick->IsSnowpack()) {
            auto snowBrick = dynamic_cast<Snowpack*>(brick);
            return new ProcessTransformSnowToIceConstant(snowBrick->GetSnowContainer());
        }
        throw ConceptionIssue(
            wxString::Format(_("Trying to apply transformation processes to unsupported brick: %s"), brick->GetName()));
    }
    if (processType == "transformation:snow_ice_swat" || processType == "transform:snow_ice_swat") {
        if (brick->IsSnowpack()) {
            auto snowBrick = dynamic_cast<Snowpack*>(brick);
            return new ProcessTransformSnowToIceSwat(snowBrick->GetSnowContainer());
        }
        throw ConceptionIssue(
            wxString::Format(_("Trying to apply transformation processes to unsupported brick: %s"), brick->GetName()));
    }
    if (processType == "transport:snow_slide") {
        if (brick->IsSnowpack()) {
            auto snowBrick = dynamic_cast<Snowpack*>(brick);
            return new ProcessLateralSnowSlide(snowBrick->GetSnowContainer());
        }
        throw ConceptionIssue(
            wxString::Format(_("Trying to apply transport processes to unsupported brick: %s"), brick->GetName()));
    }
    if (processType == "runoff:socont") {
        return new ProcessRunoffSocont(brick->GetWaterContainer());
    }
    if (processType == "infiltration:socont") {
        return new ProcessInfiltrationSocont(brick->GetWaterContainer());
    }
    if (processType == "et:socont") {
        return new ProcessETSocont(brick->GetWaterContainer());
    }
    if (processType == "melt:degree_day") {
        if (brick->IsSnowpack()) {
            auto snowBrick = dynamic_cast<Snowpack*>(brick);
            return new ProcessMeltDegreeDay(snowBrick->GetSnowContainer());
        }
        if (brick->IsGlacier()) {
            auto glacierBrick = dynamic_cast<Glacier*>(brick);
            return new ProcessMeltDegreeDay(glacierBrick->GetIceContainer());
        }
        throw ConceptionIssue(
            wxString::Format(_("Trying to apply melting processes to unsupported brick: %s"), brick->GetName()));
    }
    if (processType == "melt:degree_day_aspect") {
        if (brick->IsSnowpack()) {
            auto snowBrick = dynamic_cast<Snowpack*>(brick);
            return new ProcessMeltDegreeDayAspect(snowBrick->GetSnowContainer());
        }
        if (brick->IsGlacier()) {
            auto glacierBrick = dynamic_cast<Glacier*>(brick);
            return new ProcessMeltDegreeDayAspect(glacierBrick->GetIceContainer());
        }
        throw ConceptionIssue(
            wxString::Format(_("Trying to apply melting processes to unsupported brick: %s"), brick->GetName()));
    }
    if (processType == "melt:temperature_index") {
        if (brick->IsSnowpack()) {
            auto snowBrick = dynamic_cast<Snowpack*>(brick);
            return new ProcessMeltTemperatureIndex(snowBrick->GetSnowContainer());
        }
        if (brick->IsGlacier()) {
            auto glacierBrick = dynamic_cast<Glacier*>(brick);
            return new ProcessMeltTemperatureIndex(glacierBrick->GetIceContainer());
        }
        throw ConceptionIssue(
            wxString::Format(_("Trying to apply melting processes to unsupported brick: %s"), brick->GetName()));
    }

    throw ConceptionIssue(wxString::Format(_("Process type '%s' not recognized (Factory)."), processType));
}

bool Process::RegisterParametersAndForcing(SettingsModel* modelSettings, const string& processType) {
    if (processType == "outflow:linear") {
        ProcessOutflowLinear::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "outflow:percolation" || processType == "percolation") {
        ProcessOutflowPercolation::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "outflow:direct") {
        ProcessOutflowDirect::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "outflow:rest_direct") {
        ProcessOutflowRestDirect::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "outflow:overflow" || processType == "overflow") {
        ProcessOutflowOverflow::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "transformation:snow_ice_constant" || processType == "transform:snow_ice_constant") {
        ProcessTransformSnowToIceConstant::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "transformation:snow_ice_swat" || processType == "transform:snow_ice_swat") {
        ProcessTransformSnowToIceSwat::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "transport:snow_slide") {
        ProcessLateralSnowSlide::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "runoff:socont") {
        ProcessRunoffSocont::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "infiltration:socont") {
        ProcessInfiltrationSocont::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "et:socont") {
        ProcessETSocont::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "melt:degree_day") {
        ProcessMeltDegreeDay::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "melt:degree_day_aspect") {
        ProcessMeltDegreeDayAspect::RegisterProcessParametersAndForcing(modelSettings);
    } else if (processType == "melt:temperature_index") {
        ProcessMeltTemperatureIndex::RegisterProcessParametersAndForcing(modelSettings);
    } else {
        throw ConceptionIssue(
            wxString::Format(_("Process type '%s' not recognized (RegisterParametersAndForcing)."), processType));
    }

    return true;
}

void Process::Reset() {
    for (auto flux : _outputs) {
        flux->Reset();
    }
}

void Process::SetHydroUnitProperties(HydroUnit*, Brick*) {
    // Nothing to do...
}

void Process::SetParameters(const ProcessSettings&) {
    // Nothing to do...
}

bool Process::HasParameter(const ProcessSettings& processSettings, const string& name) {
    for (auto parameter : processSettings.parameters) {
        if (parameter->GetName() == name) {
            return true;
        }
    }

    return false;
}

float* Process::GetParameterValuePointer(const ProcessSettings& processSettings, const string& name) {
    for (auto parameter : processSettings.parameters) {
        if (parameter->GetName() == name) {
            wxASSERT(parameter->GetValuePointer());
            parameter->SetAsLinked();
            return parameter->GetValuePointer();
        }
    }

    throw MissingParameter(wxString::Format(_("The parameter '%s' could not be found."), name));
}

vecDouble Process::GetChangeRates() {
    if (_container->GetContentWithChanges() <= PRECISION) {
        vecDouble res(GetConnectionCount());
        std::fill(res.begin(), res.end(), 0);
        return res;
    }

    return GetRates();
}

void Process::StoreInOutgoingFlux(double* rate, int index) {
    wxASSERT(_outputs.size() > index);
    wxASSERT(rate);
    _outputs[index]->LinkChangeRate(rate);
}

void Process::ApplyChange(int connectionIndex, double rate, double timeStepInDays) {
    wxASSERT(_outputs.size() > connectionIndex);
    wxASSERT(rate >= 0);
    if (rate < 0) {
        wxLogError(_("Negative rate (%f) in process %s, connection %d."), rate, GetName(), connectionIndex);
        rate = 0;
    }
    if (rate > PRECISION) {
        _outputs[connectionIndex]->UpdateFlux(rate * timeStepInDays);
        _container->SubtractAmountFromDynamicContentChange(rate * timeStepInDays);
    } else {
        _outputs[connectionIndex]->UpdateFlux(0);
    }
}

double* Process::GetValuePointer(const string&) {
    return nullptr;
}

double Process::GetSumChangeRatesOtherProcesses() {
    double sumOtherProcesses = 0;

    vector<Process*> otherProcesses = _container->GetParentBrick()->GetProcesses();
    for (auto process : otherProcesses) {
        wxASSERT(process);
        if (process == this) {
            continue;
        }
        vector<Flux*> fluxes = process->GetOutputFluxes();
        for (auto flux : fluxes) {
            wxASSERT(flux);
            sumOtherProcesses += *flux->GetAmountPointer();
        }
    }

    return sumOtherProcesses;
}
