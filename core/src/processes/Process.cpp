#include "Process.h"

#include "Brick.h"
#include "Glacier.h"
#include "HydroUnit.h"
#include "ProcessETSocont.h"
#include "ProcessInfiltrationSocont.h"
#include "ProcessMeltDegreeDay.h"
#include "ProcessOutflowConstant.h"
#include "ProcessOutflowDirect.h"
#include "ProcessOutflowLinear.h"
#include "ProcessOutflowOverflow.h"
#include "ProcessOutflowRestDirect.h"
#include "ProcessRunoffSocont.h"
#include "Snowpack.h"
#include "WaterContainer.h"

Process::Process(WaterContainer* container)
    : m_container(container) {}

Process* Process::Factory(const ProcessSettings& processSettings, Brick* brick) {
    if (processSettings.type == "outflow:linear") {
        return new ProcessOutflowLinear(brick->GetWaterContainer());
    } else if (processSettings.type == "outflow:constant") {
        return new ProcessOutflowConstant(brick->GetWaterContainer());
    } else if (processSettings.type == "outflow:direct") {
        return new ProcessOutflowDirect(brick->GetWaterContainer());
    } else if (processSettings.type == "outflow:rest_direct" || processSettings.type == "outflow:RestDirect") {
        return new ProcessOutflowRestDirect(brick->GetWaterContainer());
    } else if (processSettings.type == "runoff:socont") {
        return new ProcessRunoffSocont(brick->GetWaterContainer());
    } else if (processSettings.type == "infiltration:socont") {
        return new ProcessInfiltrationSocont(brick->GetWaterContainer());
    } else if (processSettings.type == "overflow") {
        return new ProcessOutflowOverflow(brick->GetWaterContainer());
    } else if (processSettings.type == "et:socont") {
        return new ProcessETSocont(brick->GetWaterContainer());
    } else if (processSettings.type == "melt:degree_day" || processSettings.type == "Melt:DegreeDay") {
        if (brick->IsSnowpack()) {
            auto snowBrick = dynamic_cast<Snowpack*>(brick);
            return new ProcessMeltDegreeDay(snowBrick->GetSnowContainer());
        } else if (brick->IsGlacier()) {
            auto glacierBrick = dynamic_cast<Glacier*>(brick);
            return new ProcessMeltDegreeDay(glacierBrick->GetIceContainer());
        } else {
            throw ConceptionIssue(_("Trying to apply melting processes to unsupported brick."));
        }
    } else {
        wxLogError(_("Process type '%s' not recognized."), processSettings.type);
    }

    return nullptr;
}

void Process::Reset() {
    for (auto flux : m_outputs) {
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
    if (m_container->GetContentWithChanges() <= PRECISION) {
        vecDouble res(GetConnectionsNb());
        std::fill(res.begin(), res.end(), 0);
        return res;
    }

    return GetRates();
}

void Process::StoreInOutgoingFlux(double* rate, int index) {
    wxASSERT(m_outputs.size() > index);
    wxASSERT(rate);
    m_outputs[index]->LinkChangeRate(rate);
}

void Process::ApplyChange(int connectionIndex, double rate, double timeStepInDays) {
    wxASSERT(m_outputs.size() > connectionIndex);
    wxASSERT(rate >= 0);
    if (rate > PRECISION) {
        m_outputs[connectionIndex]->UpdateFlux(rate * timeStepInDays);
        m_container->SubtractAmountFromDynamicContentChange(rate * timeStepInDays);
    } else {
        m_outputs[connectionIndex]->UpdateFlux(0);
    }
}

double* Process::GetValuePointer(const string&) {
    return nullptr;
}

double Process::GetSumChangeRatesOtherProcesses() {
    double sumOtherProcesses = 0;

    vector<Process*> otherProcesses = m_container->GetParentBrick()->GetProcesses();
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
