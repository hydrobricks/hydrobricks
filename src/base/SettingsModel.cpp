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
        for (auto &brick : modelStructure.hydroUnitBricks) {
            for (auto &parameter : brick.parameters) {
                wxDELETE(parameter);
            }
        }
        for (auto &brick : modelStructure.subBasinBricks) {
            for (auto &parameter : brick.parameters) {
                wxDELETE(parameter);
            }
        }
        for (auto &splitter : modelStructure.hydroUnitSplitters) {
            for (auto &parameter : splitter.parameters) {
                wxDELETE(parameter);
            }
        }
        for (auto &splitter : modelStructure.subBasinSplitters) {
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

void SettingsModel::AddHydroUnitBrick(const wxString &name, const wxString &type) {
    wxASSERT(m_selectedStructure);

    BrickSettings brick;
    brick.name = name;
    brick.type = type;

    m_selectedStructure->hydroUnitBricks.push_back(brick);
    m_selectedBrick = &m_selectedStructure->hydroUnitBricks[m_selectedStructure->hydroUnitBricks.size() - 1];
}

void SettingsModel::AddSubBasinBrick(const wxString &name, const wxString &type) {
    wxASSERT(m_selectedStructure);

    BrickSettings brick;
    brick.name = name;
    brick.type = type;

    m_selectedStructure->subBasinBricks.push_back(brick);
    m_selectedBrick = &m_selectedStructure->subBasinBricks[m_selectedStructure->subBasinBricks.size() - 1];
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

void SettingsModel::AddBrickParameter(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedBrick);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    m_selectedBrick->parameters.push_back(parameter);
}

void SettingsModel::SetBrickParameterValue(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedBrick);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    for (auto &parameter:m_selectedBrick->parameters) {
        if (parameter->GetName().IsSameAs(name, false)) {
            parameter->SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen();
}

bool SettingsModel::BrickHasParameter(const wxString &name) {
    wxASSERT(m_selectedBrick);
    for (auto &parameter:m_selectedBrick->parameters) {
        if (parameter->GetName().IsSameAs(name, false)) {
            return true;
        }
    }

    return false;
}

void SettingsModel::AddBrickForcing(const wxString &name) {
    wxASSERT(m_selectedBrick);

    if (name.IsSameAs("Precipitation", false)) {
        m_selectedBrick->forcing.push_back(Precipitation);
    } else if (name.IsSameAs("Temperature", false)) {
        m_selectedBrick->forcing.push_back(Temperature);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void SettingsModel::AddBrickProcess(const wxString &name, const wxString &type) {
    wxASSERT(m_selectedBrick);

    ProcessSettings processSettings;
    processSettings.name = name;
    processSettings.type = type;
    m_selectedBrick->processes.push_back(processSettings);

    m_selectedProcess = &m_selectedBrick->processes[m_selectedBrick->processes.size() - 1];
}

void SettingsModel::AddProcessParameter(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedProcess);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    m_selectedProcess->parameters.push_back(parameter);
}

void SettingsModel::SetProcessParameterValue(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedProcess);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    for (auto &parameter:m_selectedProcess->parameters) {
        if (parameter->GetName().IsSameAs(name, false)) {
            parameter->SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen();
}

void SettingsModel::AddProcessForcing(const wxString &name) {
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

void SettingsModel::AddProcessOutput(const wxString &target) {
    wxASSERT(m_selectedProcess);

    OutputSettings outputSettings;
    outputSettings.target = target;
    m_selectedProcess->outputs.push_back(outputSettings);
}

void SettingsModel::OutputProcessToSameBrick() {
    wxASSERT(m_selectedBrick);
    wxASSERT(m_selectedProcess);

    OutputSettings outputSettings;
    outputSettings.target = m_selectedBrick->name;
    outputSettings.instantaneous = true;
    m_selectedProcess->outputs.push_back(outputSettings);
}

void SettingsModel::AddHydroUnitSplitter(const wxString &name, const wxString &type) {
    wxASSERT(m_selectedStructure);

    SplitterSettings splitter;
    splitter.name = name;
    splitter.type = type;

    m_selectedStructure->hydroUnitSplitters.push_back(splitter);
    m_selectedSplitter = &m_selectedStructure->hydroUnitSplitters[m_selectedStructure->hydroUnitSplitters.size() - 1];
}

void SettingsModel::AddSubBasinSplitter(const wxString &name, const wxString &type) {
    wxASSERT(m_selectedStructure);

    SplitterSettings splitter;
    splitter.name = name;
    splitter.type = type;

    m_selectedStructure->subBasinSplitters.push_back(splitter);
    m_selectedSplitter = &m_selectedStructure->subBasinSplitters[m_selectedStructure->subBasinSplitters.size() - 1];
}

void SettingsModel::AddSplitterParameter(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedSplitter);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    m_selectedSplitter->parameters.push_back(parameter);
}

void SettingsModel::SetSplitterParameterValue(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedSplitter);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    for (auto &parameter:m_selectedSplitter->parameters) {
        if (parameter->GetName().IsSameAs(name, false)) {
            parameter->SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen();
}

void SettingsModel::AddSplitterForcing(const wxString &name) {
    wxASSERT(m_selectedSplitter);

    if (name.IsSameAs("Precipitation", false)) {
        m_selectedSplitter->forcing.push_back(Precipitation);
    } else if (name.IsSameAs("Temperature", false)) {
        m_selectedSplitter->forcing.push_back(Temperature);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void SettingsModel::AddSplitterOutput(const wxString &target, const wxString &fluxType) {
    wxASSERT(m_selectedSplitter);

    OutputSettings outputSettings;
    outputSettings.target = target;
    outputSettings.fluxType = fluxType;
    m_selectedSplitter->outputs.push_back(outputSettings);
}

void SettingsModel::AddLoggingToItem(const wxString& itemName) {
    wxASSERT(m_selectedStructure);
    m_selectedStructure->logItems.push_back(itemName);
}

void SettingsModel::AddBrickLogging(const wxString& itemName) {
    wxASSERT(m_selectedBrick);
    m_selectedBrick->logItems.push_back(itemName);
}

void SettingsModel::AddProcessLogging(const wxString& itemName) {
    wxASSERT(m_selectedProcess);
    m_selectedProcess->logItems.push_back(itemName);
}

void SettingsModel::AddSplitterLogging(const wxString& itemName) {
    wxASSERT(m_selectedSplitter);
    m_selectedSplitter->logItems.push_back(itemName);
}

void SettingsModel::GeneratePrecipitationSplitters(bool withSnow) {
    wxASSERT(m_selectedStructure);

    if (withSnow) {
        // Rain/snow splitter
        AddHydroUnitSplitter("snow-rain-transition", "SnowRain");
        AddSplitterForcing("Precipitation");
        AddSplitterForcing("Temperature");
        AddSplitterOutput("rain-splitter");
        AddSplitterOutput("snow-splitter", "snow");
        AddSplitterParameter("transitionStart", 0.0f);
        AddSplitterParameter("transitionEnd", 2.0f);

        // Splitter to surfaces
        AddHydroUnitSplitter("snow-splitter", "MultiFluxes");
        AddHydroUnitSplitter("rain-splitter", "MultiFluxes");
    } else {
        // Rain splitter (connection to forcing)
        AddHydroUnitSplitter("rain", "Rain");
        AddSplitterForcing("Precipitation");
        AddSplitterOutput("rain-splitter");

        // Splitter to surfaces
        AddHydroUnitSplitter("rain-splitter", "MultiFluxes");
    }
}

void SettingsModel::GenerateSnowpacks(const wxString& snowMeltProcess) {
    wxASSERT(m_selectedStructure);

    for (const auto& brickSettings : m_selectedStructure->surfaceBricks) {
        wxString surfaceName = brickSettings.name + "-surface";

        SelectHydroUnitSplitter("snow-splitter");
        AddSplitterOutput(brickSettings.name + "-snowpack", "snow");

        AddHydroUnitBrick(brickSettings.name + "-snowpack", "Snowpack");
        AddBrickProcess("melt", snowMeltProcess);
        AddProcessOutput(surfaceName);
        if (snowMeltProcess.IsSameAs("Melt:degree-day")) {
            AddProcessForcing("Temperature");
            AddProcessParameter("degreeDayFactor", 3.0f);
            AddProcessParameter("meltingTemperature", 0.0f);
        } else {
            throw NotImplemented();
        }
    }
}

void SettingsModel::GenerateSnowpacksWithWaterRetention(const wxString& snowMeltProcess, const wxString& outflowProcess) {
    wxASSERT(m_selectedStructure);

    for (const auto& brickSettings : m_selectedStructure->surfaceBricks) {
        wxString surfaceName = brickSettings.name + "-surface";

        SelectHydroUnitSplitter("snow-splitter");
        AddSplitterOutput(brickSettings.name + "-snowpack", "snow");

        AddHydroUnitBrick(brickSettings.name + "-snowpack", "Snowpack");
        AddBrickProcess("melt", snowMeltProcess);
        AddProcessOutput(brickSettings.name + "-snowpack");

        AddBrickProcess("meltwater", outflowProcess);
        OutputProcessToSameBrick();

        if (snowMeltProcess.IsSameAs("Melt:degree-day")) {
            AddProcessForcing("Temperature");
        } else {
            throw NotImplemented();
        }
    }
}

void SettingsModel::GenerateSurfaceComponentBricks(bool withSnow) {
    wxASSERT(m_selectedStructure);

    for (int i = 0; i < int(m_selectedStructure->surfaceBricks.size()); ++i) {
        BrickSettings brickSettings = m_selectedStructure->surfaceBricks[i];
        m_selectedStructure->hydroUnitBricks.push_back(brickSettings);

        SelectHydroUnitBrick(brickSettings.name);
        SelectHydroUnitSplitter("rain-splitter");
        AddSplitterOutput(brickSettings.name);

        // Link related surface bricks
        if (withSnow) {
            AddToRelatedSurfaceBrick(brickSettings.name + "-snowpack");
        }
        if (i < m_selectedStructure->surfaceBricks.size()) {
            AddToRelatedSurfaceBrick(brickSettings.name + "-surface");
        }
    }
}

void SettingsModel::GenerateSurfaceBricks() {
    wxASSERT(m_selectedStructure);

    for (const auto& brickSettings : m_selectedStructure->surfaceBricks) {
        wxString surfaceName = brickSettings.name + "-surface";
        AddHydroUnitBrick(surfaceName, "Surface");
    }
}

bool SettingsModel::SelectStructure(int id) {
    for (auto &modelStructure : m_modelStructures) {
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
}

void SettingsModel::SelectSubBasinBrick(int index) {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    m_selectedBrick = &m_selectedStructure->subBasinBricks[index];
}

bool SettingsModel::SelectHydroUnitBrickIfFound(const wxString &name) {
    wxASSERT(m_selectedStructure);
    for (auto &brick : m_selectedStructure->hydroUnitBricks) {
        if (brick.name == name) {
            m_selectedBrick = &brick;
            return true;
        }
    }

    return false;
}

bool SettingsModel::SelectSubBasinBrickIfFound(const wxString &name) {
    wxASSERT(m_selectedStructure);
    for (auto &brick : m_selectedStructure->subBasinBricks) {
        if (brick.name == name) {
            m_selectedBrick = &brick;
            return true;
        }
    }

    return false;
}

void SettingsModel::SelectHydroUnitBrick(const wxString &name) {
    if (!SelectHydroUnitBrickIfFound(name)) {
        throw ShouldNotHappen();
    }
}

void SettingsModel::SelectSubBasinBrick(const wxString &name) {
    if (!SelectSubBasinBrickIfFound(name)) {
        throw ShouldNotHappen();
    }
}

void SettingsModel::SelectProcess(int index) {
    wxASSERT(m_selectedBrick);
    wxASSERT(!m_selectedBrick->processes.empty());

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

    throw InvalidArgument(wxString::Format(_("The process '%s' was not found."), name));
}

void SettingsModel::SelectProcessWithParameter(const wxString &name) {
    wxASSERT(m_selectedBrick);
    for (auto &process : m_selectedBrick->processes) {
        for (auto &parameter: process.parameters) {
            if (parameter->GetName().IsSameAs(name, false)) {
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

bool SettingsModel::SelectHydroUnitSplitterIfFound(const wxString &name) {
    wxASSERT(m_selectedStructure);
    for (auto &splitter : m_selectedStructure->hydroUnitSplitters) {
        if (splitter.name == name) {
            m_selectedSplitter = &splitter;
            return true;
        }
    }

    return false;
}

bool SettingsModel::SelectSubBasinSplitterIfFound(const wxString &name) {
    wxASSERT(m_selectedStructure);
    for (auto &splitter : m_selectedStructure->subBasinSplitters) {
        if (splitter.name == name) {
            m_selectedSplitter = &splitter;
            return true;
        }
    }

    return false;
}

void SettingsModel::SelectHydroUnitSplitter(const wxString &name) {
    if (!SelectHydroUnitSplitterIfFound(name)) {
        throw ShouldNotHappen();
    }
}

void SettingsModel::SelectSubBasinSplitter(const wxString &name) {
    if (!SelectSubBasinSplitterIfFound(name)) {
        throw ShouldNotHappen();
    }
}

vecStr SettingsModel::GetHydroUnitLogLabels() {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    vecStr logNames;

    for (auto &modelStructure : m_modelStructures) {
        for (auto &brick : modelStructure.hydroUnitBricks) {
            for (const auto& label : brick.logItems) {
                logNames.push_back(brick.name + ":" + label);
            }
            for (auto &process : brick.processes) {
                for (const auto& label : process.logItems) {
                    logNames.push_back(brick.name + ":" + process.name + ":" + label);
                }
            }
        }
        for (auto &splitter : modelStructure.hydroUnitSplitters) {
            for (const auto& label : splitter.logItems) {
                logNames.push_back(splitter.name + ":" + label);
            }
        }
    }

    return logNames;
}

vecStr SettingsModel::GetSubBasinLogLabels() {
    wxASSERT(m_selectedStructure);
    wxASSERT(m_modelStructures.size() == 1);

    vecStr logNames;

    for (auto &modelStructure : m_modelStructures) {
        for (auto &brick : modelStructure.subBasinBricks) {
            for (const auto& label : brick.logItems) {
                logNames.push_back(brick.name + ":" + label);
            }
            for (auto &process : brick.processes) {
                for (const auto& label : process.logItems) {
                    logNames.push_back(brick.name + ":" + process.name + ":" + label);
                }
            }
        }
        for (auto &splitter : modelStructure.subBasinSplitters) {
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

bool SettingsModel::ParseStructure(const wxString &path) {
    if (!wxFile::Exists(path)) {
        wxLogError(_("The file %s could not be found."), path);
        return false;
    }

    try {
        YAML::Node settings = YAML::LoadFile(std::string(path.mb_str()));

        if (settings["base"]) {
            wxString base = wxString(settings["base"].as<std::string>());
            if (base.IsSameAs("Socont", false)) {
                return GenerateStructureSocont(settings);
            } else {
                wxLogError(_("Model base '%s' not recognized."));
                return false;
            }
        } else {
            throw NotImplemented();
        }

    } catch(YAML::ParserException& e) {
        wxLogError(e.what());
        return false;
    }
}

bool SettingsModel::ParseParameters(const wxString &path) {
    if (!wxFile::Exists(path)) {
        wxLogError(_("The file %s could not be found."), path);
        return false;
    }

    try {
        YAML::Node root = YAML::LoadFile(std::string(path.mb_str()));

        if (!root.IsMap()) {
            wxLogError(_("Expecting a map node in the yaml parameter file."));
            return false;
        }

        for (auto itL1 = root.begin(); itL1 != root.end(); ++itL1) {
            auto keyL1 = itL1->first;
            auto valL1 = itL1->second;
            wxString nameL1 = wxString(keyL1.as<std::string>());

            bool isBrick = false;
            bool isSplitter = false;

            // Get target object
            if (SelectHydroUnitBrickIfFound(nameL1) ||
                SelectSubBasinBrickIfFound(nameL1)) {
                isBrick = true;
            } else if (SelectHydroUnitSplitterIfFound(nameL1) ||
                SelectSubBasinSplitterIfFound(nameL1)) {
                isSplitter = true;
            } else if (nameL1.IsSameAs("snowpack", false)) {
                // Specific actions needed
            } else {
                wxLogError(_("Cannot find the nameL1 '%s'."), nameL1);
                return false;
            }

            for (auto itL2 = valL1.begin(); itL2 != valL1.end(); ++itL2) {
                auto keyL2 = itL2->first;
                auto valL2 = itL2->second;
                wxString nameL2 = wxString(keyL2.as<std::string>());

                if (valL2.IsMap()) {
                    // The process to assign the parameter to has been specified.
                    for (auto itL3 = valL2.begin(); itL3 != valL2.end(); ++itL3) {
                        auto keyL3 = itL3->first;
                        auto valL3 = itL3->second;
                        wxString nameL3 = wxString(keyL3.as<std::string>());

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
                            if (nameL1.IsSameAs("snowpack", false)) {
                                for (const auto& brickSettings : m_selectedStructure->surfaceBricks) {
                                    SelectHydroUnitBrick(brickSettings.name + "-snowpack");
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
                        if (nameL1.IsSameAs("snowpack", false)) {
                            for (const auto& brickSettings : m_selectedStructure->surfaceBricks) {
                                SelectHydroUnitBrick(brickSettings.name + "-snowpack");
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
    } catch(YAML::ParserException& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}

vecStr SettingsModel::ParseSurfaceNames(const YAML::Node &settings) {
    vecStr surfaceNames;
    if (YAML::Node surfaces = settings["surfaces"]) {
        if (YAML::Node names = surfaces["names"]) {
            for (auto &&name: names) {
                surfaceNames.push_back(wxString(name.as<std::string>()));
            }
        }
    }

    return surfaceNames;
}

vecStr SettingsModel::ParseSurfaceTypes(const YAML::Node &settings) {
    vecStr surfaceTypes;
    if (YAML::Node surfaces = settings["surfaces"]) {
        if (YAML::Node types = surfaces["types"]) {
            for (auto &&type: types) {
                surfaceTypes.push_back(wxString(type.as<std::string>()));
            }
        }
    }

    return surfaceTypes;
}

wxString SettingsModel::ParseSolver(const YAML::Node &settings) {
    if (settings["solver"]) {
        return wxString(settings["solver"].as<std::string>());
    }

    return "EulerExplicit";
}

bool SettingsModel::LogAll(const YAML::Node &settings) {
    if (settings["logger"]) {
        wxString target = wxString(settings["logger"].as<std::string>());
        if (target.IsSameAs("all")) {
            return true;
        } else {
            return false;
        }
    }

    return true;
}

bool SettingsModel::GenerateStructureSocont(const YAML::Node &settings) {
    int soilStorageNb = 1;
    wxString surfaceRunoff = "socont-runoff";
    if (YAML::Node options = settings["options"]) {
        if (YAML::Node parameter = options["soil_storage_nb"]) {
            soilStorageNb = parameter.as<int>();
        }
        if (YAML::Node parameter = options["surface_runoff"]) {
            surfaceRunoff = wxString(parameter.as<std::string>());
        }
    }

    // Logger
    bool logAll = LogAll(settings);

    // Solver
    SetSolver(ParseSolver(settings));

    // Surface elements
    vecStr surfaceNames = ParseSurfaceNames(settings);
    vecStr surfaceTypes = ParseSurfaceTypes(settings);
    if (surfaceNames.size() != surfaceTypes.size()) {
        wxLogError(_("The length of the surface names and surface types do not match."));
        return false;
    }

    // Add default ground surface
    AddSurfaceBrick("ground", "GenericSurface");

    // Add other specific surfaces
    for (int i = 0; i < surfaceNames.size(); ++i) {
        wxString type = surfaceTypes[i];
        if (type.IsSameAs("ground", false)) {
            // Nothing to do, already added.
        } else if (type.IsSameAs("glacier", false)) {
            AddSurfaceBrick(surfaceNames[i], "Glacier");
        } else {
            wxLogError(_("The surface type %s is not used in Socont"), type);
            return false;
        }
    }

    GeneratePrecipitationSplitters(true);
    GenerateSnowpacks("Melt:degree-day");
    GenerateSurfaceComponentBricks(true);
    GenerateSurfaceBricks();

    // Log snow processes for surface bricks
    if (logAll) {
        for (const auto& name: surfaceNames) {
            SelectHydroUnitBrick(name + "-snowpack");
            AddBrickLogging("snow");

            SelectProcess("melt");
            AddProcessLogging("output");
        }
    }

    // Add surface-related processes
    for (int i = 0; i < surfaceNames.size(); ++i) {
        wxString type = surfaceTypes[i];
        wxString name = surfaceNames[i];
        SelectHydroUnitBrick(name);

        if (type.IsSameAs("ground", false)) {
            // Direct rain water to surface
            SelectHydroUnitBrick(name);
            AddBrickProcess("outflow-rain", "Outflow:direct");
            AddProcessOutput(name + "-surface");

        } else if (type.IsSameAs("glacier", false)) {
            // Direct snow melt to linear storage
            SelectHydroUnitBrick(name + "-surface");
            AddBrickProcess("outflow-snowmelt", "Outflow:direct");
            AddProcessOutput("glacier-area-rain-snowmelt-storage");

            // Direct rain to linear storage
            SelectHydroUnitBrick(name);
            AddBrickLogging("ice");
            AddBrickProcess("outflow-rain", "Outflow:direct");
            AddProcessOutput("glacier-area-rain-snowmelt-storage");

            // Glacier melt process
            AddBrickParameter("noMeltWhenSnowCover", 1.0);
            AddBrickParameter("infiniteStorage", 1.0);
            AddBrickProcess("melt", "Melt:degree-day");
            AddProcessForcing("Temperature");
            AddProcessParameter("degreeDayFactor", 3.0f);
            AddProcessParameter("meltingTemperature", 0.0f);
            AddProcessOutput("glacier-area-icemelt-storage");
            if (logAll) {
                AddProcessLogging("output");
            }
        }
    }

    // Basin storages for contributions from the glacierized area
    AddSubBasinBrick("glacier-area-rain-snowmelt-storage", "Storage");
    AddBrickProcess("outflow", "Outflow:linear");
    AddProcessParameter("responseFactor", 0.2f);
    AddProcessOutput("outlet");
    AddSubBasinBrick("glacier-area-icemelt-storage", "Storage");
    AddBrickProcess("outflow", "Outflow:linear");
    AddProcessParameter("responseFactor", 0.2f);
    AddProcessOutput("outlet");
    if (logAll) {
        SelectSubBasinBrick("glacier-area-rain-snowmelt-storage");
        AddBrickLogging("content");
        SelectProcess("outflow");
        AddProcessLogging("output");
        SelectSubBasinBrick("glacier-area-icemelt-storage");
        AddBrickLogging("content");
        SelectProcess("outflow");
        AddProcessLogging("output");
    }

    SelectHydroUnitBrick("ground-surface");
    AddBrickProcess("infiltration", "Infiltration:Socont");
    AddProcessOutput("slow-reservoir");
    AddBrickProcess("runoff", "Outflow:rest-direct");
    AddProcessOutput("surface-runoff");

    // Add other bricks
    if (soilStorageNb == 1) {
        AddHydroUnitBrick("slow-reservoir", "Storage");
        AddBrickProcess("ET", "ET:Socont");
        AddProcessForcing("PET");
        AddBrickProcess("outflow", "Outflow:linear");
        AddProcessOutput("outlet");
        AddBrickProcess("overflow", "Overflow");
        AddProcessOutput("outlet");
        if (logAll) {
            AddBrickLogging("content");
            AddProcessLogging("output");
            SelectProcess("ET");
            AddProcessLogging("output");
            SelectProcess("outflow");
            AddProcessLogging("output");
        }
    } else if (soilStorageNb == 2) {
        wxLogMessage(_("Using 2 soil storages."));
        AddHydroUnitBrick("slow-reservoir", "Storage");
        AddBrickParameter("capacity", 500.0f);
        AddBrickProcess("ET", "ET:Socont");
        AddProcessForcing("PET");
        AddBrickProcess("outflow", "Outflow:linear");
        AddProcessParameter("responseFactor", 0.2f);
        AddProcessOutput("outlet");
        AddBrickProcess("percolation", "Outflow:constant");
        AddProcessParameter("percolationRate", 0.4f);
        AddProcessOutput("slow-reservoir-2");
        AddBrickProcess("overflow", "Overflow");
        AddProcessOutput("outlet");
        AddHydroUnitBrick("slow-reservoir-2", "Storage");
        AddBrickProcess("outflow", "Outflow:linear");
        AddProcessParameter("responseFactor", 0.4f);
        AddProcessOutput("outlet");
        if (logAll) {
            SelectHydroUnitBrick("slow-reservoir");
            AddBrickLogging("content");
            SelectProcess("outflow");
            AddProcessParameter("responseFactor", 0.01f);
            AddProcessLogging("output");
            SelectProcess("percolation");
            AddProcessLogging("output");
            SelectHydroUnitBrick("slow-reservoir-2");
            AddBrickLogging("content");
            SelectProcess("outflow");
            AddProcessLogging("output");
        }
    } else {
        wxLogError(_("There can be only one or two groundwater storage(s)."));
    }

    AddHydroUnitBrick("surface-runoff", "Storage");
    if (surfaceRunoff.IsSameAs("socont-runoff", false)) {
        AddBrickProcess("runoff", "Runoff:Socont");
        AddProcessParameter("runoffCoefficient", 500.0f);
        AddProcessParameter("slope", 0.5f);
    } else if (surfaceRunoff.IsSameAs("linear-storage", false)) {
        wxLogMessage(_("Using a linear storage for the quick flow."));
        AddBrickProcess("outflow", "Outflow:linear");
        AddProcessParameter("responseFactor", 0.8f);
    } else {
        wxLogError(_("The surface runoff option %s is not recognised in Socont."), surfaceRunoff);
        return false;
    }

    AddProcessOutput("outlet");
    if (logAll) {
        AddBrickLogging("content");
        AddProcessLogging("output");
    }

    AddLoggingToItem("outlet");

    return true;
}