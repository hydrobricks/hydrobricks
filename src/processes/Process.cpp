#include "Process.h"

#include "Brick.h"
#include "ProcessOutflowLinear.h"
#include "ProcessETSocont.h"

Process::Process(Brick* brick)
    : m_brick(brick)
{
    if (brick) {
        brick->AddProcess(this);
    }
}

Process* Process::Factory(const ProcessSettings &processSettings, Brick* brick) {
    if (processSettings.type.IsSameAs("Outflow:linear")) {
        auto process = new ProcessOutflowLinear(brick);
        process->AssignParameters(processSettings);
        return process;
    } else if (processSettings.type.IsSameAs("ET:Socont")) {
        auto process = new ProcessETSocont(brick);
        return process;
    } else {
        wxLogError(_("Process type '%s' not recognized."), processSettings.type);
    }

    return nullptr;
}

void Process::AssignParameters(const ProcessSettings &processSettings) {
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

void Process::ApplyChange(int connectionIndex, double rate, double timeStepInDays) {
    wxASSERT(m_outputs.size() > connectionIndex);
    m_outputs[connectionIndex]->UpdateFlux(rate * timeStepInDays);
    m_brick->SubtractAmount(rate * timeStepInDays);
}

double* Process::GetValuePointer(const wxString&) {
    return nullptr;
}