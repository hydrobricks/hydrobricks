#include "ParameterSet.h"

#include "Parameter.h"

ParameterSet::ParameterSet()
    : m_selectedStructure(nullptr),
      m_selectedBrick(nullptr)
{
    ModelStructure initialStructure;
    initialStructure.id = 1;
    m_modelStructures.push_back(initialStructure);
    m_selectedStructure = &m_modelStructures[0];
}

ParameterSet::~ParameterSet() {
    for (auto &modelStructure : m_modelStructures) {
        for (auto &brick : modelStructure.bricks) {
            for (int k = 0; k < brick.parameters.size(); ++k) {
                wxDELETE(brick.parameters[k]);
            }
        }
    }
}

void ParameterSet::SetSolver(const wxString &solverName) {
    m_solver.name = solverName;
}

void ParameterSet::SetTimer(const wxString &start, const wxString &end, int timeStep, const wxString &timeStepUnit) {
    m_timer.start = start;
    m_timer.end = end;
    m_timer.timeStep = timeStep;
    m_timer.timeStepUnit = timeStepUnit;
}

void ParameterSet::AddBrick(const wxString &name, const wxString &type) {
    wxASSERT(m_selectedStructure);

    BrickSettings brick;
    brick.name = name;
    brick.type = type;

    m_selectedStructure->bricks.push_back(brick);
    m_selectedBrick = &m_selectedStructure->bricks[m_selectedStructure->bricks.size()-1];
}

void ParameterSet::AddParameterToCurrentBrick(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedBrick);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    m_selectedBrick->parameters.push_back(parameter);
}

void ParameterSet::AddForcingToCurrentBrick(const wxString &name) {
    wxASSERT(m_selectedBrick);

    if (name.IsSameAs("Precipitation", false)) {
        m_selectedBrick->forcing.push_back(Precipitation);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void ParameterSet::AddOutputToCurrentBrick(const wxString &target, const wxString &type) {
    wxASSERT(m_selectedBrick);
    BrickOutputSettings outputSettings;
    outputSettings.target = target;
    outputSettings.type = type;
    m_selectedBrick->outputs.push_back(outputSettings);
}

bool ParameterSet::SelectStructure(int id) {
    for (auto &modelStructure : m_modelStructures) {
        if (modelStructure.id == id) {
            m_selectedStructure = &modelStructure;
            if (m_selectedStructure->bricks.empty()) {
                m_selectedBrick = nullptr;
            } else {
                m_selectedBrick = &m_selectedStructure->bricks[0];
            }
            return true;
        }
    }

    return false;
}
