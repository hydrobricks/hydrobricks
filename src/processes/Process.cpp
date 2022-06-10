#include "Process.h"

#include "Brick.h"
#include "WaterContainer.h"
#include "ProcessETSocont.h"
#include "ProcessMeltDegreeDay.h"
#include "ProcessOutflowConstant.h"
#include "ProcessOutflowDirect.h"
#include "ProcessOutflowLinear.h"
#include "ProcessOutflowOverflow.h"
#include "Snowpack.h"
#include "Glacier.h"
#include "ProcessInfiltrationSocont.h"
#include "ProcessOutflowRestDirect.h"

Process::Process(WaterContainer* container)
    : m_container(container)
{
}

Process* Process::Factory(const ProcessSettings &processSettings, Brick* brick) {
    if (processSettings.type.IsSameAs("Outflow:linear", false)) {
        auto process = new ProcessOutflowLinear(brick->GetWaterContainer());
        process->AssignParameters(processSettings);
        return process;
    } else if (processSettings.type.IsSameAs("Outflow:constant", false)) {
        auto process = new ProcessOutflowConstant(brick->GetWaterContainer());
        process->AssignParameters(processSettings);
        return process;
    } else if (processSettings.type.IsSameAs("Outflow:direct", false)) {
        return new ProcessOutflowDirect(brick->GetWaterContainer());
    } else if (processSettings.type.IsSameAs("Outflow:rest-direct", false) ||
               processSettings.type.IsSameAs("Outflow:RestDirect", false)) {
        return new ProcessOutflowRestDirect(brick->GetWaterContainer());
    } else if (processSettings.type.IsSameAs("Infiltration:Socont", false)) {
        return new ProcessInfiltrationSocont(brick->GetWaterContainer());
    } else if (processSettings.type.IsSameAs("Overflow", false)) {
        return new ProcessOutflowOverflow(brick->GetWaterContainer());
    } else if (processSettings.type.IsSameAs("ET:Socont", false)) {
        return new ProcessETSocont(brick->GetWaterContainer());
    } else if (processSettings.type.IsSameAs("Melt:degree-day", false) ||
               processSettings.type.IsSameAs("Melt:DegreeDay", false)) {
        if (brick->IsSnowpack()) {
            auto snowBrick = dynamic_cast<Snowpack*>(brick);
            auto process = new ProcessMeltDegreeDay(snowBrick->GetSnowContainer());
            process->AssignParameters(processSettings);
            return process;
        } else if (brick->IsGlacier()) {
            auto glacierBrick = dynamic_cast<Glacier*>(brick);
            auto process = new ProcessMeltDegreeDay(glacierBrick->GetIceContainer());
            process->AssignParameters(processSettings);
            return process;
        } else {
            throw ConceptionIssue(_("Trying to apply melting processes to unsupported brick."));
        }
    } else {
        wxLogError(_("Process type '%s' not recognized."), processSettings.type);
    }

    return nullptr;
}

void Process::AssignParameters(const ProcessSettings &) {
    // Nothing to do...
}

bool Process::HasParameter(const ProcessSettings &processSettings, const wxString &name) {
    for (auto parameter: processSettings.parameters) {
        if (parameter->GetName().IsSameAs(name, false)) {
            return true;
        }
    }

    return false;
}

float* Process::GetParameterValuePointer(const ProcessSettings &processSettings, const wxString &name) {
    for (auto parameter: processSettings.parameters) {
        if (parameter->GetName().IsSameAs(name, false)) {
            wxASSERT(parameter->GetValuePointer());
            parameter->SetAsLinked();
            return parameter->GetValuePointer();
        }
    }

    throw MissingParameter(wxString::Format(_("The parameter '%s' could not be found."), name));
}

void Process::StoreInOutgoingFlux(double* rate, int index) {
    wxASSERT(m_outputs.size() > index);
    m_outputs[index]->LinkChangeRate(rate);
}

void Process::ApplyChange(int connectionIndex, double rate, double timeStepInDays) {
    wxASSERT(m_outputs.size() > connectionIndex);
    m_outputs[connectionIndex]->UpdateFlux(rate * timeStepInDays);
    m_container->SubtractAmount(rate * timeStepInDays);
}

double* Process::GetValuePointer(const wxString&) {
    return nullptr;
}

void Process::SetOutputFluxesFraction(double value) {
    for (auto output : m_outputs) {
        if (output->NeedsWeighting()) {
            output->MultiplyFraction(value);
        }
    }
}

double Process::GetSumChangeRatesOtherProcesses() {
    double sumOtherProcesses = 0;

    std::vector<Process *> otherProcesses = m_container->GetParentBrick()->GetProcesses();
    for (auto process: otherProcesses) {
        wxASSERT(process);
        if (process == this) {
            continue;
        }
        std::vector<Flux *> fluxes = process->GetOutputFluxes();
        for (auto flux: fluxes) {
            wxASSERT(flux);
            sumOtherProcesses+= *flux->GetAmountPointer();
        }
    }

    return sumOtherProcesses;
}