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

void SettingsModel::AddBrickParameter(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedBrick);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    m_selectedBrick->parameters.push_back(parameter);
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

void SettingsModel::AddProcessOutput(const wxString &target, bool withWeighting) {
    wxASSERT(m_selectedProcess);

    OutputSettings outputSettings;
    outputSettings.target = target;
    outputSettings.withWeighting = withWeighting;
    m_selectedProcess->outputs.push_back(outputSettings);
}

void SettingsModel::OutputProcessToSameBrick() {
    wxASSERT(m_selectedBrick);
    wxASSERT(m_selectedProcess);

    OutputSettings outputSettings;
    outputSettings.target = m_selectedBrick->name;
    outputSettings.withWeighting = false;
    outputSettings.instantaneous = true;
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

void SettingsModel::AddSplitterParameter(const wxString &name, float value, const wxString &type) {
    wxASSERT(m_selectedSplitter);

    if (!type.IsSameAs("Constant")) {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    m_selectedSplitter->parameters.push_back(parameter);
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
        AddSplitter("snow-rain", "SnowRain");
        AddSplitterForcing("Precipitation");
        AddSplitterForcing("Temperature");
        AddSplitterOutput("rain-splitter");
        AddSplitterOutput("snow-splitter", "snow");

        // Splitter to surfaces
        AddSplitter("snow-splitter", "MultiFluxes");
        AddSplitter("rain-splitter", "MultiFluxes");
    } else {
        // Rain splitter (connection to forcing)
        AddSplitter("rain", "Rain");
        AddSplitterForcing("Precipitation");
        AddSplitterOutput("rain-splitter");

        // Splitter to surfaces
        AddSplitter("rain-splitter", "MultiFluxes");
    }
}

void SettingsModel::GenerateSnowpacks(const wxString& snowMeltProcess) {
    wxASSERT(m_selectedStructure);

    for (const auto& brickSettings : m_selectedStructure->surfaceBricks) {
        wxString surfaceName = brickSettings.name + "-surface";

        SelectSplitter("snow-splitter");
        AddSplitterOutput(brickSettings.name + "-snowpack", "snow");

        AddBrick(brickSettings.name + "-snowpack", "Snowpack");
        AddBrickProcess("melt", snowMeltProcess);
        AddProcessOutput(surfaceName);
        if (snowMeltProcess.IsSameAs("Melt:degree-day")) {
            AddProcessForcing("Temperature");
        } else {
            throw NotImplemented();
        }
    }
}

void SettingsModel::GenerateSnowpacksWithWaterRetention(const wxString& snowMeltProcess, const wxString& outflowProcess) {
    wxASSERT(m_selectedStructure);

    for (const auto& brickSettings : m_selectedStructure->surfaceBricks) {
        wxString surfaceName = brickSettings.name + "-surface";

        SelectSplitter("snow-splitter");
        AddSplitterOutput(brickSettings.name + "-snowpack", "snow");

        AddBrick(brickSettings.name + "-snowpack", "Snowpack");
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
        m_selectedStructure->bricks.push_back(brickSettings);

        SelectBrick(brickSettings.name);
        SelectSplitter("rain-splitter");
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

        if (settings["base"]) {
            wxString base = wxString(settings["base"].as<std::string>());
            if (base.IsSameAs("Socont", false)) {
                return GenerateStructureSocont(settings);
            } else {
                wxLogError(_("Model base '%s' not recognized."));
                return false;
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
    if (YAML::Node options = settings["options"]) {
        if (YAML::Node parameter = options["soil_storage_nb"]) {
            soilStorageNb = parameter.as<int>();
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
            SelectBrick(name + "-snowpack");
            AddBrickLogging("content");

            SelectProcess("melt");
            AddProcessLogging("output");
        }
    }

    // Add surface-related processes
    for (int i = 0; i < surfaceNames.size(); ++i) {
        wxString type = surfaceTypes[i];
        wxString name = surfaceNames[i];
        SelectBrick(name);

        if (type.IsSameAs("ground", false)) {
            // Direct rain water to surface
            SelectBrick(name);
            AddBrickProcess("outflow-rain", "Outflow:direct");
            AddProcessOutput(name + "-surface");

        } else if (type.IsSameAs("glacier", false)) {
            // Direct snow melt to linear storage
            SelectBrick(name + "-surface");
            AddBrickProcess("outflow-snowmelt", "Outflow:direct");
            AddProcessOutput("glacier-area-rain-snowmelt-storage");

            // Direct rain to linear storage
            SelectBrick(name);
            AddBrickProcess("outflow-rain", "Outflow:direct");
            AddProcessOutput("glacier-area-rain-snowmelt-storage");

            // Glacier melt process
            AddBrickProcess("melt", "Melt:degree-day");
            AddProcessForcing("Temperature");
            AddProcessOutput("glacier-area-icemelt-storage");
            if (logAll) {
                AddProcessLogging("output");
            }
        }
    }

    // Basin storages for contributions from the glacierized area
    AddBrick("glacier-area-rain-snowmelt-storage", "Storage");
    AddBrickProcess("outflow", "Outflow:linear");
    AddProcessOutput("outlet");
    AddBrick("glacier-area-icemelt-storage", "Storage");
    AddBrickProcess("outflow", "Outflow:linear");
    AddProcessOutput("outlet");
    if (logAll) {
        SelectBrick("glacier-area-rain-snowmelt-storage");
        AddBrickLogging("content");
        SelectProcess("outflow");
        AddProcessLogging("output");
        SelectBrick("glacier-area-icemelt-storage");
        AddBrickLogging("content");
        SelectProcess("outflow");
        AddProcessLogging("output");
    }



    SelectBrick("ground-surface");



    // Add other bricks
    if (soilStorageNb == 1) {
        AddBrick("slow-reservoir", "Storage");
        AddBrickProcess("outflow", "Outflow:linear");
        if (logAll) {
            AddBrickLogging("content");
            AddProcessLogging("output");
        }
    } else if (soilStorageNb == 2) {
        AddBrick("slow-reservoir-1", "Storage");
        AddBrickProcess("outflow", "Outflow:linear");
        AddProcessOutput("outlet");
        AddBrickProcess("percolation", "Outflow:constant");
        AddProcessOutput("slow-reservoir-2");
        AddBrick("slow-reservoir-2", "Storage");
        AddBrickProcess("outflow", "Outflow:linear");
        AddProcessOutput("outlet");
        if (logAll) {
            SelectBrick("slow-reservoir-1");
            AddBrickLogging("content");
            SelectProcess("outflow");
            AddProcessLogging("output");
            SelectProcess("percolation");
            AddProcessLogging("output");
            SelectBrick("slow-reservoir-2");
            AddBrickLogging("content");
            SelectProcess("outflow");
            AddProcessLogging("output");
        }
    } else {
        wxLogError(_("There can be only one or two groundwater storage(s)."));
    }




    AddLoggingToItem("outlet");

    return true;
}