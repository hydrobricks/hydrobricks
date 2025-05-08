#include "SettingsModel.h"

#include "Parameter.h"
#include "Process.h"

SettingsModel::SettingsModel()
    : m_logAll(false),
      m_selectedStructure(nullptr),
      m_selectedBrick(nullptr),
      m_selectedProcess(nullptr),
      m_selectedSplitter(nullptr) {
    ModelStructure initialStructure;
    initialStructure.id = 1;
    m_modelStructures.push_back(initialStructure);
    m_selectedStructure = &m_modelStructures[0];
}

SettingsModel::~SettingsModel() {
    for (auto& modelStructure : m_modelStructures) {
        for (auto& brick : modelStructure.hydroUnitBricks) {
            for (auto& parameter : brick.parameters) {
                wxDELETE(parameter);
            }
        }
        for (auto& brick : modelStructure.subBasinBricks) {
            for (auto& parameter : brick.parameters) {
                wxDELETE(parameter);
            }
        }
        for (auto& splitter : modelStructure.hydroUnitSplitters) {
            for (auto& parameter : splitter.parameters) {
                wxDELETE(parameter);
            }
        }
        for (auto& splitter : modelStructure.subBasinSplitters) {
            for (auto& parameter : splitter.parameters) {
                wxDELETE(parameter);
            }
        }
    }
}

void SettingsModel::SetSolver(const string& solverName) {
    m_solver.name = solverName;
}

void SettingsModel::SetTimer(const string& start, const string& end, int timeStep, const string& timeStepUnit) {
    m_timer.start = start;
    m_timer.end = end;
    m_timer.timeStep = timeStep;
    m_timer.timeStepUnit = timeStepUnit;
}

void SettingsModel::AddHydroUnitBrick(const string& name, const string& type) {
    wxASSERT(m_selectedStructure);

    BrickSettings brick;
    brick.name = name;
    brick.type = type;

    m_selectedStructure->hydroUnitBricks.push_back(brick);
    m_selectedBrick = &m_selectedStructure->hydroUnitBricks[m_selectedStructure->hydroUnitBricks.size() - 1];

    if (m_logAll) {
        AddBrickLogging("water_content");
        if (type == "glacier") {
            AddBrickLogging("ice_content");
        } else if (type == "snowpack") {
            AddBrickLogging("snow_content");
        }
    }
}

void SettingsModel::AddSubBasinBrick(const string& name, const string& type) {
    wxASSERT(m_selectedStructure);

    BrickSettings brick;
    brick.name = name;
    brick.type = type;

    m_selectedStructure->subBasinBricks.push_back(brick);
    m_selectedBrick = &m_selectedStructure->subBasinBricks[m_selectedStructure->subBasinBricks.size() - 1];

    if (m_logAll) {
        AddBrickLogging("water_content");
        if (type == "glacier") {
            AddBrickLogging("ice_content");
        } else if (type == "snowpack") {
            AddBrickLogging("snow_content");
        }
    }
}

void SettingsModel::AddLandCoverBrick(const string& name, const string& type) {
    wxASSERT(m_selectedStructure);

    AddHydroUnitBrick(name, type);
    m_selectedStructure->landCoverBricks.push_back(m_selectedStructure->hydroUnitBricks.size() - 1);

    if (SelectHydroUnitSplitterIfFound("rain_splitter")) {
        AddSplitterOutput(name);
    }
}

void SettingsModel::AddSurfaceComponentBrick(const string& name, const string& type) {
    wxASSERT(m_selectedStructure);

    AddHydroUnitBrick(name, type);
    m_selectedStructure->surfaceComponentBricks.push_back(m_selectedStructure->hydroUnitBricks.size() - 1);
}

void SettingsModel::SetSurfaceComponentParent(const string& name) {
    wxASSERT(m_selectedBrick);
    m_selectedBrick->parent = name;
}

void SettingsModel::AddBrickParameter(const string& name, float value, const string& type) {
    wxASSERT(m_selectedBrick);

    if (type != "constant") {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    m_selectedBrick->parameters.push_back(parameter);
}

void SettingsModel::SetBrickParameterValue(const string& name, float value, const string& type) {
    wxASSERT(m_selectedBrick);

    if (type != "constant") {
        throw NotImplemented();
    }

    for (auto& parameter : m_selectedBrick->parameters) {
        if (parameter->GetName() == name) {
            parameter->SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen();
}

bool SettingsModel::BrickHasParameter(const string& name) {
    wxASSERT(m_selectedBrick);
    for (auto& parameter : m_selectedBrick->parameters) {
        if (parameter->GetName() == name) {
            return true;
        }
    }

    return false;
}

void SettingsModel::AddBrickForcing(const string& name) {
    wxASSERT(m_selectedBrick);

    if (name == "precipitation") {
        m_selectedBrick->forcing.push_back(Precipitation);
    } else if (name == "temperature") {
        m_selectedBrick->forcing.push_back(Temperature);
    } else if (name == "solar_radiation" || name == "r_solar") {
        m_selectedBrick->forcing.push_back(Radiation);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void SettingsModel::AddBrickProcess(const string& name, const string& type, const string& target, bool log) {
    wxASSERT(m_selectedBrick);

    wxLogVerbose(_("Adding brick process to brick: %s, process name: %s, process type: %s, target: %s"),
                 m_selectedBrick->name, name, type, target);

    ProcessSettings processSettings;
    processSettings.name = name;
    processSettings.type = type;
    m_selectedBrick->processes.push_back(processSettings);

    m_selectedProcess = &m_selectedBrick->processes[m_selectedBrick->processes.size() - 1];

    if (!target.empty()) {
        // If the target contains ":", separate into target and fluxType
        auto pos = target.find(':');
        if (pos != string::npos) {
            string fluxType = target.substr(pos + 1);
            string targetSub = target.substr(0, pos);
            AddProcessOutput(targetSub, fluxType);
        } else {
            AddProcessOutput(target);
        }
    }
    if (log || m_logAll) {
        AddProcessLogging("output");
    }

    // Register the related parameters
    if (!Process::RegisterParametersAndForcing(this, processSettings.type)) {
        throw InvalidArgument(wxString::Format(_("Fail to register the parameters and forcing for the process '%s'."),
                                               processSettings.type));
    }
}

void SettingsModel::AddProcessParameter(const string& name, float value, const string& type) {
    wxASSERT(m_selectedProcess);

    if (type != "constant") {
        throw NotImplemented();
    }

    // If the parameter already exists, replace its value
    for (auto& parameter : m_selectedProcess->parameters) {
        if (parameter->GetName() == name) {
            parameter->SetValue(value);
            return;
        }
    }

    auto parameter = new Parameter(name, value);

    m_selectedProcess->parameters.push_back(parameter);
}

void SettingsModel::SetProcessParameterValue(const string& name, float value, const string& type) {
    wxASSERT(m_selectedProcess);

    if (type != "constant") {
        throw NotImplemented();
    }

    for (auto& parameter : m_selectedProcess->parameters) {
        if (parameter->GetName() == name) {
            parameter->SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen();
}

void SettingsModel::AddProcessForcing(const string& name) {
    wxASSERT(m_selectedProcess);

    if (name == "precipitation") {
        m_selectedProcess->forcing.push_back(Precipitation);
    } else if (name == "pet") {
        m_selectedProcess->forcing.push_back(PET);
    } else if (name == "temperature") {
        m_selectedProcess->forcing.push_back(Temperature);
    } else if (name == "solar_radiation" || name == "r_solar") {
        m_selectedProcess->forcing.push_back(Radiation);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void SettingsModel::AddProcessOutput(const string& target, const string& fluxType) {
    wxASSERT(m_selectedProcess);

    OutputSettings outputSettings;
    outputSettings.target = target;
    outputSettings.fluxType = fluxType;
    m_selectedProcess->outputs.push_back(outputSettings);
}

void SettingsModel::SetProcessOutputsAsInstantaneous() {
    wxASSERT(m_selectedProcess);

    for (auto& output : m_selectedProcess->outputs) {
        output.isInstantaneous = true;
    }
}

void SettingsModel::SetProcessOutputsAsStatic() {
    wxASSERT(m_selectedProcess);

    for (auto& output : m_selectedProcess->outputs) {
        output.isStatic = true;
    }
}

void SettingsModel::OutputProcessToSameBrick() {
    wxASSERT(m_selectedBrick);
    wxASSERT(m_selectedProcess);

    OutputSettings outputSettings;
    outputSettings.target = m_selectedBrick->name;
    outputSettings.isInstantaneous = true;
    m_selectedProcess->outputs.push_back(outputSettings);
}

void SettingsModel::AddHydroUnitSplitter(const string& name, const string& type) {
    wxASSERT(m_selectedStructure);

    SplitterSettings splitter;
    splitter.name = name;
    splitter.type = type;

    m_selectedStructure->hydroUnitSplitters.push_back(splitter);
    m_selectedSplitter = &m_selectedStructure->hydroUnitSplitters[m_selectedStructure->hydroUnitSplitters.size() - 1];
}

void SettingsModel::AddSubBasinSplitter(const string& name, const string& type) {
    wxASSERT(m_selectedStructure);

    SplitterSettings splitter;
    splitter.name = name;
    splitter.type = type;

    m_selectedStructure->subBasinSplitters.push_back(splitter);
    m_selectedSplitter = &m_selectedStructure->subBasinSplitters[m_selectedStructure->subBasinSplitters.size() - 1];
}

void SettingsModel::AddSplitterParameter(const string& name, float value, const string& type) {
    wxASSERT(m_selectedSplitter);

    if (type != "constant") {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    m_selectedSplitter->parameters.push_back(parameter);
}

void SettingsModel::SetSplitterParameterValue(const string& name, float value, const string& type) {
    wxASSERT(m_selectedSplitter);

    if (type != "constant") {
        throw NotImplemented();
    }

    for (auto& parameter : m_selectedSplitter->parameters) {
        if (parameter->GetName() == name) {
            parameter->SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen();
}

void SettingsModel::AddSplitterForcing(const string& name) {
    wxASSERT(m_selectedSplitter);

    if (name == "precipitation") {
        m_selectedSplitter->forcing.push_back(Precipitation);
    } else if (name == "temperature") {
        m_selectedSplitter->forcing.push_back(Temperature);
    } else if (name == "solar_radiation" || name == "r_solar") {
        m_selectedSplitter->forcing.push_back(Radiation);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void SettingsModel::AddSplitterOutput(const string& target, const string& fluxType) {
    wxASSERT(m_selectedSplitter);

    OutputSettings outputSettings;
    outputSettings.target = target;
    outputSettings.fluxType = fluxType;
    m_selectedSplitter->outputs.push_back(outputSettings);
}

void SettingsModel::AddLoggingToItem(const string& itemName) {
    wxASSERT(m_selectedStructure);
    if (std::find(m_selectedStructure->logItems.begin(), m_selectedStructure->logItems.end(), itemName) !=
        m_selectedStructure->logItems.end()) {
        return;
    }
    m_selectedStructure->logItems.push_back(itemName);
}

void SettingsModel::AddLoggingToItems(std::initializer_list<const string> items) {
    wxASSERT(m_selectedStructure);
    for (const auto& item : items) {
        if (std::find(m_selectedStructure->logItems.begin(), m_selectedStructure->logItems.end(), item) !=
            m_selectedStructure->logItems.end()) {
            continue;
        }
        m_selectedStructure->logItems.push_back(item);
    }
}

void SettingsModel::AddBrickLogging(const string& itemName) {
    wxASSERT(m_selectedBrick);
    if (std::find(m_selectedBrick->logItems.begin(), m_selectedBrick->logItems.end(), itemName) !=
        m_selectedBrick->logItems.end()) {
        return;
    }
    m_selectedBrick->logItems.push_back(itemName);
}

void SettingsModel::AddBrickLogging(std::initializer_list<const string> items) {
    wxASSERT(m_selectedBrick);
    for (const auto& item : items) {
        if (std::find(m_selectedBrick->logItems.begin(), m_selectedBrick->logItems.end(), item) !=
            m_selectedBrick->logItems.end()) {
            continue;
        }
        m_selectedBrick->logItems.push_back(item);
    }
}

void SettingsModel::AddProcessLogging(const string& itemName) {
    wxASSERT(m_selectedProcess);
    if (std::find(m_selectedProcess->logItems.begin(), m_selectedProcess->logItems.end(), itemName) !=
        m_selectedProcess->logItems.end()) {
        return;
    }
    m_selectedProcess->logItems.push_back(itemName);
}

void SettingsModel::AddSplitterLogging(const string& itemName) {
    wxASSERT(m_selectedSplitter);
    if (std::find(m_selectedSplitter->logItems.begin(), m_selectedSplitter->logItems.end(), itemName) !=
        m_selectedSplitter->logItems.end()) {
        return;
    }
    m_selectedSplitter->logItems.push_back(itemName);
}

void SettingsModel::GeneratePrecipitationSplitters(bool withSnow) {
    wxASSERT(m_selectedStructure);

    if (withSnow) {
        // Rain/snow splitter
        AddHydroUnitSplitter("snow_rain_transition", "snow_rain");
        AddSplitterForcing("precipitation");
        AddSplitterForcing("temperature");
        AddSplitterOutput("rain_splitter");
        AddSplitterOutput("snow_splitter", "snow");
        AddSplitterParameter("transition_start", 0.0f);
        AddSplitterParameter("transition_end", 2.0f);

        // Splitter to land covers
        AddHydroUnitSplitter("snow_splitter", "multi_fluxes");
        AddHydroUnitSplitter("rain_splitter", "multi_fluxes");
    } else {
        // Rain splitter (connection to forcing)
        AddHydroUnitSplitter("rain", "rain");
        AddSplitterForcing("precipitation");
        AddSplitterOutput("rain_splitter");

        // Splitter to land covers
        AddHydroUnitSplitter("rain_splitter", "multi_fluxes");
    }
}

void SettingsModel::GenerateSnowpacks(const string& snowMeltProcess) {
    wxASSERT(m_selectedStructure);

    for (int brickSettingsIndex : m_selectedStructure->landCoverBricks) {
        BrickSettings brickSettings = m_selectedStructure->hydroUnitBricks[brickSettingsIndex];
        SelectHydroUnitSplitter("snow_splitter");
        AddSplitterOutput(brickSettings.name + "_snowpack", "snow");
        AddSurfaceComponentBrick(brickSettings.name + "_snowpack", "snowpack");
        SetSurfaceComponentParent(brickSettings.name);

        AddBrickProcess("melt", snowMeltProcess, brickSettings.name);
    }
}

void SettingsModel::GenerateSnowpacksWithWaterRetention(const string& snowMeltProcess, const string& outflowProcess) {
    wxASSERT(m_selectedStructure);

    for (int brickSettingsIndex : m_selectedStructure->landCoverBricks) {
        BrickSettings brickSettings = m_selectedStructure->hydroUnitBricks[brickSettingsIndex];
        SelectHydroUnitSplitter("snow_splitter");
        AddSplitterOutput(brickSettings.name + "_snowpack", "snow");
        AddSurfaceComponentBrick(brickSettings.name + "_snowpack", "snowpack");
        SetSurfaceComponentParent(brickSettings.name);

        AddBrickProcess("melt", snowMeltProcess);
        OutputProcessToSameBrick();

        AddBrickProcess("meltwater", outflowProcess);
        AddProcessOutput(brickSettings.name);
    }
}

bool SettingsModel::SelectStructure(int id) {
    for (auto& modelStructure : m_modelStructures) {
        if (modelStructure.id == id) {
            m_selectedStructure = &modelStructure;
            if (m_selectedStructure->hydroUnitBricks.empty()) {
                m_selectedBrick = nullptr;
            } else {
                m_selectedBrick = &m_selectedStructure->hydroUnitBricks[0];
            }
            return true;
        }
    }

    return false;
}

void SettingsModel::SelectHydroUnitBrick(int index) {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    m_selectedBrick = &m_selectedStructure->hydroUnitBricks[index];
    m_selectedProcess = nullptr;
}

void SettingsModel::SelectSubBasinBrick(int index) {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    m_selectedBrick = &m_selectedStructure->subBasinBricks[index];
    m_selectedProcess = nullptr;
}

bool SettingsModel::SelectHydroUnitBrickIfFound(const string& name) {
    wxASSERT(m_selectedStructure);
    for (auto& brick : m_selectedStructure->hydroUnitBricks) {
        if (brick.name == name) {
            m_selectedBrick = &brick;
            m_selectedProcess = nullptr;
            return true;
        }
    }

    return false;
}

bool SettingsModel::SelectSubBasinBrickIfFound(const string& name) {
    wxASSERT(m_selectedStructure);
    for (auto& brick : m_selectedStructure->subBasinBricks) {
        if (brick.name == name) {
            m_selectedBrick = &brick;
            m_selectedProcess = nullptr;
            return true;
        }
    }

    return false;
}

void SettingsModel::SelectHydroUnitBrick(const string& name) {
    if (!SelectHydroUnitBrickIfFound(name)) {
        throw NotFound(wxString::Format("The hydro unit brick '%s' was not found", name));
    }
    m_selectedProcess = nullptr;
}

void SettingsModel::SelectHydroUnitBrickByName(const string& name) {
    if (!SelectHydroUnitBrickIfFound(name)) {
        throw NotFound(wxString::Format("The hydro unit brick '%s' was not found", name));
    }
    m_selectedProcess = nullptr;
}

void SettingsModel::SelectSubBasinBrick(const string& name) {
    if (!SelectSubBasinBrickIfFound(name)) {
        throw NotFound(wxString::Format("The sub-basin brick '%s' was not found", name));
    }
    m_selectedProcess = nullptr;
}

void SettingsModel::SelectProcess(int index) {
    wxASSERT(m_selectedBrick);
    wxASSERT(!m_selectedBrick->processes.empty());

    m_selectedProcess = &m_selectedBrick->processes[index];
}

void SettingsModel::SelectProcess(const string& name) {
    wxASSERT(m_selectedBrick);
    for (auto& process : m_selectedBrick->processes) {
        if (process.name == name) {
            m_selectedProcess = &process;
            return;
        }
    }

    throw InvalidArgument(wxString::Format(_("The process '%s' was not found."), name));
}

void SettingsModel::SelectProcessWithParameter(const string& name) {
    wxASSERT(m_selectedBrick);
    for (auto& process : m_selectedBrick->processes) {
        for (auto& parameter : process.parameters) {
            if (parameter->GetName() == name) {
                m_selectedProcess = &process;
                return;
            }
        }
    }

    throw InvalidArgument(wxString::Format(_("The parameter '%s' was not found."), name));
}

void SettingsModel::SelectHydroUnitSplitter(int index) {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    m_selectedSplitter = &m_selectedStructure->hydroUnitSplitters[index];
}

void SettingsModel::SelectSubBasinSplitter(int index) {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    m_selectedSplitter = &m_selectedStructure->subBasinSplitters[index];
}

bool SettingsModel::SelectHydroUnitSplitterIfFound(const string& name) {
    wxASSERT(m_selectedStructure);
    for (auto& splitter : m_selectedStructure->hydroUnitSplitters) {
        if (splitter.name == name) {
            m_selectedSplitter = &splitter;
            return true;
        }
    }

    return false;
}

bool SettingsModel::SelectSubBasinSplitterIfFound(const string& name) {
    wxASSERT(m_selectedStructure);
    for (auto& splitter : m_selectedStructure->subBasinSplitters) {
        if (splitter.name == name) {
            m_selectedSplitter = &splitter;
            return true;
        }
    }

    return false;
}

void SettingsModel::SelectHydroUnitSplitter(const string& name) {
    if (!SelectHydroUnitSplitterIfFound(name)) {
        throw NotFound(wxString::Format("The hydro unit splitter '%s' was not found", name));
    }
}

void SettingsModel::SelectSubBasinSplitter(const string& name) {
    if (!SelectSubBasinSplitterIfFound(name)) {
        throw NotFound(wxString::Format("The sub-basin splitter '%s' was not found", name));
    }
}

vecStr SettingsModel::GetHydroUnitLogLabels() {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    vecStr logNames;

    for (auto& modelStructure : m_modelStructures) {
        for (auto& brick : modelStructure.hydroUnitBricks) {
            for (const auto& label : brick.logItems) {
                logNames.push_back(brick.name + ":" + label);
            }
            for (auto& process : brick.processes) {
                for (const auto& label : process.logItems) {
                    logNames.push_back(brick.name + ":" + process.name + ":" + label);
                }
            }
        }
        for (auto& splitter : modelStructure.hydroUnitSplitters) {
            for (const auto& label : splitter.logItems) {
                logNames.push_back(splitter.name + ":" + label);
            }
        }
    }

    return logNames;
}

vecStr SettingsModel::GetLandCoverBricksNames() const {
    wxASSERT(m_selectedStructure);
    vecStr names;
    for (int index : m_selectedStructure->landCoverBricks) {
        names.push_back(m_selectedStructure->hydroUnitBricks[index].name);
    }

    return names;
}

vecStr SettingsModel::GetSubBasinLogLabels() {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    vecStr logNames;

    for (auto& modelStructure : m_modelStructures) {
        for (auto& brick : modelStructure.subBasinBricks) {
            for (const auto& label : brick.logItems) {
                logNames.push_back(brick.name + ":" + label);
            }
            for (auto& process : brick.processes) {
                for (const auto& label : process.logItems) {
                    logNames.push_back(brick.name + ":" + process.name + ":" + label);
                }
            }
        }
        for (auto& splitter : modelStructure.subBasinSplitters) {
            for (const auto& label : splitter.logItems) {
                logNames.push_back(splitter.name + ":" + label);
            }
        }
    }

    for (const auto& label : m_selectedStructure->logItems) {
        logNames.push_back(label);
    }

    return logNames;
}

vecStr SettingsModel::GetSubBasinGenericLogLabels() {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    vecStr logNames;

    for (const auto& label : m_selectedStructure->logItems) {
        logNames.push_back(label);
    }

    return logNames;
}

bool SettingsModel::ParseStructure(const string&) {
    wxLogError(_("This function is outdated and should not be used anymore."));
    return false;
}

bool SettingsModel::ParseParameters(const string&) {
    wxLogError(_("This function is outdated and should not be used anymore."));
    return false;
    /*
        if (!wxFile::Exists(path)) {
            wxLogError(_("The file %s could not be found."), path);
            return false;
        }

        try {
            YAML::Node root = YAML::LoadFile(path);

            if (!root.IsMap()) {
                wxLogError(_("Expecting a map node in the yaml parameter file."));
                return false;
            }

            for (auto itL1 = root.begin(); itL1 != root.end(); ++itL1) {
                auto keyL1 = itL1->first;
                auto valL1 = itL1->second;
                string nameL1 = keyL1.as<string>();

                bool isBrick = false;
                bool isSplitter = false;

                // Get target object
                if (SelectHydroUnitBrickIfFound(nameL1) || SelectSubBasinBrickIfFound(nameL1)) {
                    isBrick = true;
                } else if (SelectHydroUnitSplitterIfFound(nameL1) || SelectSubBasinSplitterIfFound(nameL1)) {
                    isSplitter = true;
                } else if (nameL1 == "snowpack") {
                    // Specific actions needed
                } else {
                    wxLogError(_("Cannot find the nameL1 '%s'."), nameL1);
                    return false;
                }

                for (auto itL2 = valL1.begin(); itL2 != valL1.end(); ++itL2) {
                    auto keyL2 = itL2->first;
                    auto valL2 = itL2->second;
                    string nameL2 = keyL2.as<string>();

                    if (valL2.IsMap()) {
                        // The process to assign the parameter to has been specified.
                        for (auto itL3 = valL2.begin(); itL3 != valL2.end(); ++itL3) {
                            auto keyL3 = itL3->first;
                            auto valL3 = itL3->second;
                            string nameL3 = keyL3.as<string>();

                            if (!valL3.IsScalar()) {
                                throw ShouldNotHappen();
                            }

                            auto paramValue = valL3.as<float>();

                            if (isBrick) {
                                SelectProcess(nameL2);
                                SetProcessParameterValue(nameL3, paramValue);
                            } else if (isSplitter) {
                                throw ShouldNotHappen();
                            } else {
                                if (nameL1 == "snowpack") {
                                    for (int index : m_selectedStructure->landCoverBricks) {
                                        BrickSettings brickSettings = m_selectedStructure->hydroUnitBricks[index];
                                        SelectHydroUnitBrick(brickSettings.name + "_snowpack");
                                        SelectProcess(nameL2);
                                        SetProcessParameterValue(nameL3, paramValue);
                                    }
                                }
                            }
                        }
                    } else if (valL2.IsScalar()) {
                        // Can be: brick, splitter or process parameter.
                        auto paramValue = valL2.as<float>();

                        if (isBrick) {
                            if (BrickHasParameter(nameL2)) {
                                SetBrickParameterValue(nameL2, paramValue);
                            } else {
                                SelectProcessWithParameter(nameL2);
                                SetProcessParameterValue(nameL2, paramValue);
                            }
                        } else if (isSplitter) {
                            SetSplitterParameterValue(nameL2, paramValue);
                        } else {
                            if (nameL1 == "snowpack") {
                                for (int index : m_selectedStructure->landCoverBricks) {
                                    BrickSettings brickSettings = m_selectedStructure->hydroUnitBricks[index];
                                    SelectHydroUnitBrick(brickSettings.name + "_snowpack");
                                    SelectProcessWithParameter(nameL2);
                                    SetProcessParameterValue(nameL2, paramValue);
                                }
                            }
                        }
                    } else {
                        throw ShouldNotHappen();
                    }
                }
            }
        } catch (YAML::ParserException& e) {
            wxLogError(e.what());
            return false;
        }

        return true;*/
}

bool SettingsModel::SetParameterValue(const string& component, const string& name, float value) {
    // Check if the parameter should be set for multiple components
    if (component.find(',') != string::npos) {
        wxArrayString components = wxSplit(wxString(component), ',');
        for (const auto& componentItem : components) {
            if (!SetParameterValue(componentItem.ToStdString(), name, value)) {
                wxLogError(_("Fail to set the parameter '%s' for the component '%s'."), name, componentItem);
                return false;
            }
        }
        return true;
    }

    // Get target object
    if (SelectHydroUnitBrickIfFound(component) || SelectSubBasinBrickIfFound(component)) {
        if (BrickHasParameter(name)) {
            SetBrickParameterValue(name, value);
        } else {
            SelectProcessWithParameter(name);
            SetProcessParameterValue(name, value);
        }
        return true;
    } else if (SelectHydroUnitSplitterIfFound(component) || SelectSubBasinSplitterIfFound(component)) {
        SetSplitterParameterValue(name, value);
        return true;
    } else if (component.find("type:") != string::npos) {
        wxString type = wxString(component).AfterFirst(':');

        // Set the parameter for all bricks of the given type - hydro units
        for (auto& brick : m_selectedStructure->hydroUnitBricks) {
            if (brick.type == type) {
                SelectHydroUnitBrick(brick.name);
                if (BrickHasParameter(name)) {
                    SetBrickParameterValue(name, value);
                } else {
                    SelectProcessWithParameter(name);
                    SetProcessParameterValue(name, value);
                }
            }
        }

        // Set the parameter for all bricks of the given type - subbasins
        for (auto& brick : m_selectedStructure->subBasinBricks) {
            if (brick.type == type) {
                SelectHydroUnitBrick(brick.name);
                if (BrickHasParameter(name)) {
                    SetBrickParameterValue(name, value);
                } else {
                    SelectProcessWithParameter(name);
                    SetProcessParameterValue(name, value);
                }
            }
        }

    } else {
        wxLogError(_("Cannot find the component '%s'."), component);
        return false;
    }

    return true;
}

vecStr SettingsModel::ParseLandCoverNames(const YAML::Node& settings) {
    vecStr landCoverNames;
    if (YAML::Node landCovers = settings["land_covers"]) {
        if (YAML::Node names = landCovers["names"]) {
            for (auto&& name : names) {
                landCoverNames.push_back(name.as<string>());
            }
        }
    }

    return landCoverNames;
}

vecStr SettingsModel::ParseLandCoverTypes(const YAML::Node& settings) {
    vecStr landCoverTypes;
    if (YAML::Node landCovers = settings["land_covers"]) {
        if (YAML::Node types = landCovers["types"]) {
            for (auto&& type : types) {
                landCoverTypes.push_back(type.as<string>());
            }
        }
    }

    return landCoverTypes;
}

string SettingsModel::ParseSolver(const YAML::Node& settings) {
    if (settings["solver"]) {
        return settings["solver"].as<string>();
    }

    return "euler_explicit";
}

bool SettingsModel::LogAll(const YAML::Node& settings) {
    if (settings["logger"]) {
        string target = settings["logger"].as<string>();
        if (target == "all") {
            return true;
        } else {
            return false;
        }
    }

    return true;
}