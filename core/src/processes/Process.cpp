#include "Process.h"

#include "Brick.h"
#include "Glacier.h"
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

Process::Process(WaterContainer *container) : m_container(container) {}

Process *Process::Factory(const ProcessSettings &processSettings, Brick *brick) {
    if (processSettings.type == "Outflow:linear") {
        auto process = new ProcessOutflowLinear(brick->GetWaterContainer());
        process->AssignParameters(processSettings);
        return process;
    } else if (processSettings.type == "Outflow:constant") {
        auto process = new ProcessOutflowConstant(brick->GetWaterContainer());
        process->AssignParameters(processSettings);
        return process;
    } else if (processSettings.type == "Outflow:direct") {
        return new ProcessOutflowDirect(brick->GetWaterContainer());
    } else if (processSettings.type == "Outflow:rest-direct" || processSettings.type == "Outflow:RestDirect") {
        return new ProcessOutflowRestDirect(brick->GetWaterContainer());
    } else if (processSettings.type == "Runoff:Socont") {
        auto process = new ProcessRunoffSocont(brick->GetWaterContainer());
        process->AssignParameters(processSettings);
        return process;
    } else if (processSettings.type == "Infiltration:Socont") {
        return new ProcessInfiltrationSocont(brick->GetWaterContainer());
    } else if (processSettings.type == "Overflow") {
        return new ProcessOutflowOverflow(brick->GetWaterContainer());
    } else if (processSettings.type == "ET:Socont") {
        return new ProcessETSocont(brick->GetWaterContainer());
    } else if (processSettings.type == "Melt:degree-day" || processSettings.type == "Melt:DegreeDay") {
        if (brick->IsSnowpack()) {
            auto snowBrick = dynamic_cast<Snowpack *>(brick);
            auto process = new ProcessMeltDegreeDay(snowBrick->GetSnowContainer());
            process->AssignParameters(processSettings);
            return process;
        } else if (brick->IsGlacier()) {
            auto glacierBrick = dynamic_cast<Glacier *>(brick);
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

bool Process::HasParameter(const ProcessSettings &processSettings, const std::string &name) {
    for (auto parameter : processSettings.parameters) {
        if (parameter->GetName() == name) {
            return true;
        }
    }

    return false;
}

float *Process::GetParameterValuePointer(const ProcessSettings &processSettings, const std::string &name) {
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

void Process::StoreInOutgoingFlux(double *rate, int index) {
    wxASSERT(m_outputs.size() > index);
    m_outputs[index]->LinkChangeRate(rate);
}

void Process::ApplyChange(int connectionIndex, double rate, double timeStepInDays) {
    wxASSERT(m_outputs.size() > connectionIndex);
    wxASSERT(rate >= 0);
    if (rate > PRECISION) {
        m_outputs[connectionIndex]->UpdateFlux(rate * timeStepInDays);
        m_container->SubtractAmount(rate * timeStepInDays);
    } else {
        m_outputs[connectionIndex]->UpdateFlux(0);
    }
}

double *Process::GetValuePointer(const std::string &) {
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
    for (auto process : otherProcesses) {
        wxASSERT(process);
        if (process == this) {
            continue;
        }
        std::vector<Flux *> fluxes = process->GetOutputFluxes();
        for (auto flux : fluxes) {
            wxASSERT(flux);
            sumOtherProcesses += *flux->GetAmountPointer();
        }
    }

    return sumOtherProcesses;
}
