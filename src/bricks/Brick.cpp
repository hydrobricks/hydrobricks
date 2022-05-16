#include "Brick.h"

#include "Glacier.h"
#include "HydroUnit.h"
#include "Snowpack.h"
#include "Storage.h"
#include "Surface.h"

Brick::Brick(HydroUnit *hydroUnit)
    : m_needsSolver(true),
      m_content(0),
      m_contentChange(0),
      m_capacity(nullptr),
      m_hydroUnit(hydroUnit),
      m_overflow(nullptr)
{
    if (hydroUnit) {
        hydroUnit->AddBrick(this);
    }
}

Brick* Brick::Factory(const BrickSettings &brickSettings, HydroUnit* unit) {
    if (brickSettings.type.IsSameAs("Storage")) {
        auto brick = new Storage(unit);
        brick->AssignParameters(brickSettings);
        return brick;
    } else if (brickSettings.type.IsSameAs("Surface")) {
        auto brick = new Surface(unit);
        brick->AssignParameters(brickSettings);
        return brick;
    } else if (brickSettings.type.IsSameAs("Glacier")) {
        auto brick = new Glacier(unit);
        brick->AssignParameters(brickSettings);
        return brick;
    } else if (brickSettings.type.IsSameAs("Snowpack")) {
        auto brick = new Snowpack(unit);
        brick->AssignParameters(brickSettings);
        return brick;
    } else {
        wxLogError(_("Brick type '%s' not recognized."), brickSettings.type);
    }

    return nullptr;
}

bool Brick::IsOk() {
    if (m_hydroUnit == nullptr) {
        wxLogError(_("The brick is not attached to a hydro unit."));
        return false;
    }
    if (m_inputs.empty()) {
        wxLogError(_("The brick is not attached to inputs."));
        return false;
    }
    for (auto process : m_processes) {
        if (!process->IsOk()) {
            return false;
        }
    }

    return true;
}

void Brick::AssignParameters(const BrickSettings &brickSettings) {
    if (HasParameter(brickSettings, "capacity")) {
        m_capacity = GetParameterValuePointer(brickSettings, "capacity");
    }
}

bool Brick::HasParameter(const BrickSettings &brickSettings, const wxString &name) {
    for (auto parameter: brickSettings.parameters) {
        if (parameter->GetName().IsSameAs(name, false)) {
            return true;
        }
    }

    return false;
}

float* Brick::GetParameterValuePointer(const BrickSettings &brickSettings, const wxString &name) {
    for (auto parameter: brickSettings.parameters) {
        if (parameter->GetName().IsSameAs(name, false)) {
            wxASSERT(parameter->GetValuePointer());
            parameter->SetAsLinked();
            return parameter->GetValuePointer();
        }
    }

    throw MissingParameter(wxString::Format(_("The parameter '%s' could not be found."), name));
}

Process* Brick::GetProcess(int index) {
    wxASSERT(m_processes.size() > index);
    wxASSERT(m_processes[index]);

    return m_processes[index];
}

void Brick::SubtractAmount(double change) {
    m_contentChange -= change;
}

void Brick::AddAmount(double change) {
    m_contentChange += change;
}

void Brick::Finalize() {
    m_content += m_contentChange;
    m_contentChange = 0;
    wxASSERT(m_content >= 0);
}

double Brick::SumIncomingFluxes() {
    wxASSERT(!m_inputs.empty());

    double sum = 0;
    for (auto & input : m_inputs) {
        sum += input->GetAmount();
    }

    return sum;
}

void Brick::UpdateContentFromInputs() {
    m_contentChange += SumIncomingFluxes();
}

void Brick::ApplyConstraints(double timeStep) {
    // Get outgoing change rates
    vecDoublePt outgoingRates;
    double outputs = 0;
    for (auto process : m_processes) {
        for (auto flux : process->GetOutputFluxes()) {
            double* changeRate = flux->GetChangeRatePointer();
            wxASSERT(changeRate != nullptr);
            outgoingRates.push_back(changeRate);
            outputs += *changeRate;
        }
    }

    // Get incoming change rates
    vecDoublePt incomingRates;
    double inputs = 0;
    double inputsStatic = 0;
    for (auto & input : m_inputs) {
        if (input->IsForcing()) {
            inputsStatic += input->GetAmount();
        } else if (input->IsStatic()) {
            inputsStatic += input->GetAmount();
        } else {
            double* changeRate = input->GetChangeRatePointer();
            wxASSERT(changeRate != nullptr);
            incomingRates.push_back(changeRate);
            inputs += *changeRate;
        }
    }

    double change = inputs - outputs;

    // Avoid negative content
    if (change < 0 && GetContentWithChanges() + inputsStatic + change * timeStep < 0) {
        double diff = (GetContentWithChanges() + inputsStatic + change * timeStep) / timeStep;
        // Limit the different rates proportionally
        for (auto rate :outgoingRates) {
            if (*rate == 0) {
                continue;
            }
            *rate += diff * std::abs((*rate) / outputs);
        }
    }

    // Enforce maximum capacity
    if (HasMaximumCapacity()) {
        if (GetContentWithChanges() + inputsStatic + change * timeStep > *m_capacity) {
            double diff = (GetContentWithChanges() + inputsStatic + change * timeStep - *m_capacity) / timeStep;
            // If it has an overflow, use it
            if (HasOverflow()) {
                *(m_overflow->GetOutputFluxes()[0]->GetChangeRatePointer()) = diff;
                return;
            }
            // Check that it is not only due to forcing
            if (GetContentWithChanges() + inputsStatic > *m_capacity) {
                throw ConceptionIssue(_("Forcing is coming directly into a brick with limited capacity and no overflow."));
            }
            // Limit the different rates proportionally
            for (auto rate :incomingRates) {
                if (*rate == 0) {
                    continue;
                }
                *rate -= diff * std::abs((*rate) / inputs);
            }
        }
    }
}

vecDoublePt Brick::GetStateVariableChanges() {
    return vecDoublePt {&m_contentChange};
}

vecDoublePt Brick::GetStateVariableChangesFromProcesses() {
    vecDoublePt values;
    for (auto const &process: m_processes) {
        vecDoublePt processValues = process->GetStateVariables();

        if (processValues.empty()) {
            continue;
        }
        for (auto const &value : processValues) {
            values.push_back(value);
        }
    }

    return values;
}

int Brick::GetProcessesConnectionsNb() {
    int counter = 0;

    for (auto const &process: m_processes) {
        counter += process->GetConnectionsNb();
    }

    return counter;
}

double* Brick::GetBaseValuePointer(const wxString& name) {
    if (name.IsSameAs("content")) {
        return &m_content;
    }

    return nullptr;
}

double* Brick::GetValuePointer(const wxString&) {
    return nullptr;
}