#include "yaml-cpp/yaml.h"

#include "Parameter.h"
#include "SettingsModel.h"

SettingsModel::SettingsModel()
    : m_selectedStructure(nullptr),
      m_selectedBrick(nullptr),
      m_selectedProcess(nullptr),
      m_selectedSplitter(nullptr)
{
    ModelStructure initialStructure;
    initialStructure.id = 1;
    m_modelStructures.push_back(initialStructure);
    m_selectedStructure = &m_modelStructures[0];
}

SettingsModel::~SettingsModel() {
    for (auto &modelStructure : m_modelStructures) {
        for (auto &brick : modelStructure.bricks) {
            for (auto &parameter : brick.parameters) {
                wxDELETE(parameter);
            }
        }
        for (auto &splitter : modelStructure.splitters) {
            for (auto &parameter : splitter.parameters) {
                wxDELETE(parameter);
            }
        }
    }
}

void SettingsModel::SetSolver(const wxString &solverName) {
    m_solver.name = solverName;
}

void SettingsModel::SetTimer(const wxString &start, const wxString &end, int timeStep, const wxString &timeStepUnit) {
    m_timer.start = start;
    m_timer.end = end;
    m_timer.timeStep = timeStep;
    m_timer.timeStepUnit = timeStepUnit;
}

void SettingsModel::AddBrick(const wxString &name, const wxString &type) {
    wxASSERT(m_selectedStructure);

    BrickSettings brick;
    brick.name = name;
    brick.type = type;

    m_selectedStructure->bricks.push_back(brick);
    m_selectedBrick = &m_selectedStructure->bricks[m_selectedStructure->bricks.size() - 1];
}

void SettingsModel::AddSurfaceBrick(const wxString &name, const wxString &type) {
    wxASSERT(m_selectedStructure);

    BrickSettings brick;
    brick.name = name;
    brick.type = type;

    m_selectedStructure->surfaceBricks.push_back(brick);
}

void SettingsModel::AddToRelatedSurfaceBrick(const wxString &name) {
    wxASSERT(m_selectedBrick);
    m_selectedBrick->relatedSurfaceBricks.push_back(name);
}

void SettingsModel::AddParameterToCurrentBrick(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedBrick);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    m_selectedBrick->parameters.push_back(parameter);
}

void SettingsModel::AddForcingToCurrentBrick(const wxString &name) {
    wxASSERT(m_selectedBrick);

    if (name.IsSameAs("Precipitation", false)) {
        m_selectedBrick->forcing.push_back(Precipitation);
    } else if (name.IsSameAs("Temperature", false)) {
        m_selectedBrick->forcing.push_back(Temperature);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void SettingsModel::AddProcessToCurrentBrick(const wxString &name, const wxString &type) {
    wxASSERT(m_selectedBrick);

    ProcessSettings processSettings;
    processSettings.name = name;
    processSettings.type = type;
    m_selectedBrick->processes.push_back(processSettings);

    m_selectedProcess = &m_selectedBrick->processes[m_selectedBrick->processes.size() - 1];
}

void SettingsModel::AddParameterToCurrentProcess(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedProcess);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    m_selectedProcess->parameters.push_back(parameter);
}

void SettingsModel::AddForcingToCurrentProcess(const wxString &name) {
    wxASSERT(m_selectedProcess);

    if (name.IsSameAs("Precipitation", false)) {
        m_selectedProcess->forcing.push_back(Precipitation);
    } else if (name.IsSameAs("PET", false)) {
        m_selectedProcess->forcing.push_back(PET);
    } else if (name.IsSameAs("Temperature", false)) {
        m_selectedProcess->forcing.push_back(Temperature);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void SettingsModel::AddOutputToCurrentProcess(const wxString &target, bool withWeighting) {
    wxASSERT(m_selectedProcess);

    OutputSettings outputSettings;
    outputSettings.target = target;
    outputSettings.withWeighting = withWeighting;
    m_selectedProcess->outputs.push_back(outputSettings);
}

void SettingsModel::AddSplitter(const wxString &name, const wxString &type) {
    wxASSERT(m_selectedStructure);

    SplitterSettings splitter;
    splitter.name = name;
    splitter.type = type;

    m_selectedStructure->splitters.push_back(splitter);
    m_selectedSplitter = &m_selectedStructure->splitters[m_selectedStructure->splitters.size() - 1];
}

void SettingsModel::AddParameterToCurrentSplitter(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedSplitter);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    m_selectedSplitter->parameters.push_back(parameter);
}

void SettingsModel::AddForcingToCurrentSplitter(const wxString &name) {
    wxASSERT(m_selectedSplitter);

    if (name.IsSameAs("Precipitation", false)) {
        m_selectedSplitter->forcing.push_back(Precipitation);
    } else if (name.IsSameAs("Temperature", false)) {
        m_selectedSplitter->forcing.push_back(Temperature);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void SettingsModel::AddOutputToCurrentSplitter(const wxString &target) {
    wxASSERT(m_selectedSplitter);

    OutputSettings outputSettings;
    outputSettings.target = target;
    m_selectedSplitter->outputs.push_back(outputSettings);
}

void SettingsModel::AddLoggingToItem(const wxString& itemName) {
    wxASSERT(m_selectedStructure);
    m_selectedStructure->logItems.push_back(itemName);
}

void SettingsModel::AddLoggingToCurrentBrick(const wxString& itemName) {
    wxASSERT(m_selectedBrick);
    m_selectedBrick->logItems.push_back(itemName);
}

void SettingsModel::AddLoggingToCurrentProcess(const wxString& itemName) {
    wxASSERT(m_selectedProcess);
    m_selectedProcess->logItems.push_back(itemName);
}

void SettingsModel::AddLoggingToCurrentSplitter(const wxString& itemName) {
    wxASSERT(m_selectedSplitter);
    m_selectedSplitter->logItems.push_back(itemName);
}

void SettingsModel::EnableSnow(const wxString &meltProcess) {
    wxASSERT(m_selectedStructure);
    m_selectedStructure->withSnow = true;
    m_selectedStructure->snowMeltProcess = meltProcess;
}

void SettingsModel::GenerateSurfaceComponents() {
    wxASSERT(m_selectedStructure);

    int surfacesNb = int(m_selectedStructure->surfaceBricks.size());

    // Create precipitation splitters
    if (m_selectedStructure->withSnow) {
        // Rain/snow splitter
        AddSplitter("snow-rain", "SnowRain");
        AddForcingToCurrentSplitter("Precipitation");
        AddForcingToCurrentSplitter("Temperature");
        AddOutputToCurrentSplitter("rain-splitter");
        AddOutputToCurrentSplitter("snow-splitter");

        // Splitter to surfaces
        AddSplitter("snow-splitter", "MultiFluxes");
        AddSplitter("rain-splitter", "MultiFluxes");
    } else {
        // Rain splitter (connection to forcing)
        AddSplitter("rain", "Rain");
        AddForcingToCurrentSplitter("Precipitation");
        AddOutputToCurrentSplitter("rain-splitter");

        // Splitter to surfaces
        AddSplitter("rain-splitter", "MultiFluxes");
    }

    // Create snowpacks
    if (m_selectedStructure->withSnow) {
        for (int i = 0; i < surfacesNb; ++i) {
            BrickSettings brickSettings = m_selectedStructure->surfaceBricks[i];
            wxString surfaceName = brickSettings.name + "-surface";

            SelectSplitter("snow-splitter");
            AddOutputToCurrentSplitter(brickSettings.name + "-snowpack");

            AddBrick(brickSettings.name + "-snowpack", "Snowpack");
            AddProcessToCurrentBrick("melt", m_selectedStructure->snowMeltProcess);
            AddOutputToCurrentProcess(surfaceName);
            if (m_selectedStructure->snowMeltProcess.IsSameAs("Melt:degree-day")) {
                AddForcingToCurrentProcess("Temperature");
            } else {
                throw NotImplemented();
            }
        }
    }

    // Create bricks
    for (int i = 0; i < surfacesNb; ++i) {
        BrickSettings brickSettings = m_selectedStructure->surfaceBricks[i];
        m_selectedStructure->bricks.push_back(brickSettings);

        SelectBrick(brickSettings.name);
        SelectSplitter("rain-splitter");
        AddOutputToCurrentSplitter(brickSettings.name);

        // Link related surface bricks
        wxString surfaceName = brickSettings.name + "-surface";
        if (m_selectedStructure->withSnow) {
            AddToRelatedSurfaceBrick(brickSettings.name + "-snowpack");
        }
        if (i < m_selectedStructure->surfaceBricks.size()) {
            AddToRelatedSurfaceBrick(surfaceName);
        }
    }

    // Create surface bricks
    for (int i = 0; i < surfacesNb; ++i) {
        BrickSettings brickSettings = m_selectedStructure->surfaceBricks[i];
        wxString surfaceName = brickSettings.name + "-surface";
        AddBrick(surfaceName, "Surface");
    }
}

bool SettingsModel::SelectStructure(int id) {
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

void SettingsModel::SelectBrick(int index) {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    m_selectedBrick = &m_selectedStructure->bricks[index];
}

void SettingsModel::SelectBrick(const wxString &name) {
    wxASSERT(m_selectedStructure);
    for (auto &brick : m_selectedStructure->bricks) {
        if (brick.name == name) {
            m_selectedBrick = &brick;
            return;
        }
    }

    throw ShouldNotHappen();
}

void SettingsModel::SelectProcess(int index) {
    wxASSERT(m_selectedBrick);
    wxASSERT(m_selectedBrick->processes.size() >= 1);

    m_selectedProcess = &m_selectedBrick->processes[index];
}

void SettingsModel::SelectProcess(const wxString &name) {
    wxASSERT(m_selectedBrick);
    for (auto &process : m_selectedBrick->processes) {
        if (process.name == name) {
            m_selectedProcess = &process;
            return;
        }
    }

    throw ShouldNotHappen();
}

void SettingsModel::SelectSplitter(int index) {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    m_selectedSplitter = &m_selectedStructure->splitters[index];
}

void SettingsModel::SelectSplitter(const wxString &name) {
    wxASSERT(m_selectedStructure);
    for (auto &splitter : m_selectedStructure->splitters) {
        if (splitter.name == name) {
            m_selectedSplitter = &splitter;
            return;
        }
    }

    throw ShouldNotHappen();
}

vecStr SettingsModel::GetHydroUnitLogLabels() {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    vecStr logNames;

    for (auto &modelStructure : m_modelStructures) {
        for (auto &brick : modelStructure.bricks) {
            for (const auto& label : brick.logItems) {
                logNames.push_back(brick.name + ":" + label);
            }
            for (auto &process : brick.processes) {
                for (const auto& label : process.logItems) {
                    logNames.push_back(brick.name + ":" + process.name + ":" + label);
                }
            }
        }
        for (auto &splitter : modelStructure.splitters) {
            for (const auto& label : splitter.logItems) {
                logNames.push_back(splitter.name + ":" + label);
            }
        }
    }

    return logNames;
}

vecStr SettingsModel::GetAggregatedLogLabels() {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    return m_selectedStructure->logItems;
}

bool SettingsModel::Parse(const wxString &path) {
    if (!wxFile::Exists(path)) {
        wxLogError(_("The file %s could not be found."), path);
        return false;
    }

    try {
        YAML::Node settings = YAML::LoadFile(std::string(path.mb_str()));

        if (YAML::Node parameter = settings["base"]) {
            const std::string base = parameter.as<std::string>();
        }

    } catch(YAML::ParserException& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}