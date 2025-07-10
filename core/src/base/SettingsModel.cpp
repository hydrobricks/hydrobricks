#include "SettingsModel.h"

#include "Parameter.h"
#include "Process.h"

SettingsModel::SettingsModel()
    : _logAll(false),
      _selectedStructure(nullptr),
      _selectedBrick(nullptr),
      _selectedProcess(nullptr),
      _selectedSplitter(nullptr) {
    ModelStructure initialStructure;
    initialStructure.id = 1;
    _modelStructures.push_back(initialStructure);
    _selectedStructure = &_modelStructures[0];
}

SettingsModel::~SettingsModel() {
    for (auto& modelStructure : _modelStructures) {
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
    _solver.name = solverName;
}

void SettingsModel::SetTimer(const string& start, const string& end, int timeStep, const string& timeStepUnit) {
    _timer.start = start;
    _timer.end = end;
    _timer.timeStep = timeStep;
    _timer.timeStepUnit = timeStepUnit;
}

void SettingsModel::AddHydroUnitBrick(const string& name, const string& type) {
    wxASSERT(_selectedStructure);

    BrickSettings brick;
    brick.name = name;
    brick.type = type;

    _selectedStructure->hydroUnitBricks.push_back(brick);
    _selectedBrick = &_selectedStructure->hydroUnitBricks[_selectedStructure->hydroUnitBricks.size() - 1];

    if (_logAll) {
        AddBrickLogging("water_content");
        if (type == "glacier") {
            AddBrickLogging("ice_content");
        } else if (type == "snowpack") {
            AddBrickLogging("snow_content");
        }
    }
}

void SettingsModel::AddSubBasinBrick(const string& name, const string& type) {
    wxASSERT(_selectedStructure);

    BrickSettings brick;
    brick.name = name;
    brick.type = type;

    _selectedStructure->subBasinBricks.push_back(brick);
    _selectedBrick = &_selectedStructure->subBasinBricks[_selectedStructure->subBasinBricks.size() - 1];

    if (_logAll) {
        AddBrickLogging("water_content");
        if (type == "glacier") {
            AddBrickLogging("ice_content");
        } else if (type == "snowpack") {
            AddBrickLogging("snow_content");
        }
    }
}

void SettingsModel::AddLandCoverBrick(const string& name, const string& type) {
    wxASSERT(_selectedStructure);

    AddHydroUnitBrick(name, type);
    _selectedStructure->landCoverBricks.push_back(_selectedStructure->hydroUnitBricks.size() - 1);

    if (SelectHydroUnitSplitterIfFound("rain_splitter")) {
        AddSplitterOutput(name);
    }
}

void SettingsModel::AddSurfaceComponentBrick(const string& name, const string& type) {
    wxASSERT(_selectedStructure);

    AddHydroUnitBrick(name, type);
    _selectedStructure->surfaceComponentBricks.push_back(_selectedStructure->hydroUnitBricks.size() - 1);
}

void SettingsModel::SetSurfaceComponentParent(const string& name) {
    wxASSERT(_selectedBrick);
    _selectedBrick->parent = name;
}

void SettingsModel::AddBrickParameter(const string& name, float value, const string& type) {
    wxASSERT(_selectedBrick);

    if (type != "constant") {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    _selectedBrick->parameters.push_back(parameter);
}

void SettingsModel::SetBrickParameterValue(const string& name, float value, const string& type) {
    wxASSERT(_selectedBrick);

    if (type != "constant") {
        throw NotImplemented();
    }

    for (auto& parameter : _selectedBrick->parameters) {
        if (parameter->GetName() == name) {
            parameter->SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen();
}

bool SettingsModel::BrickHasParameter(const string& name) {
    wxASSERT(_selectedBrick);
    for (auto& parameter : _selectedBrick->parameters) {
        if (parameter->GetName() == name) {
            return true;
        }
    }

    return false;
}

void SettingsModel::AddBrickForcing(const string& name) {
    wxASSERT(_selectedBrick);

    if (name == "precipitation") {
        _selectedBrick->forcing.push_back(Precipitation);
    } else if (name == "temperature") {
        _selectedBrick->forcing.push_back(Temperature);
    } else if (name == "solar_radiation" || name == "r_solar") {
        _selectedBrick->forcing.push_back(Radiation);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void SettingsModel::AddBrickProcess(const string& name, const string& type, const string& target, bool log) {
    wxASSERT(_selectedBrick);

    wxLogVerbose(_("Adding brick process to brick: %s, process name: %s, process type: %s, target: %s"),
                 _selectedBrick->name, name, type, target);

    ProcessSettings processSettings;
    processSettings.name = name;
    processSettings.type = type;
    _selectedBrick->processes.push_back(processSettings);

    _selectedProcess = &_selectedBrick->processes[_selectedBrick->processes.size() - 1];

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
    if (log || _logAll) {
        AddProcessLogging("output");
    }

    // Register the related parameters
    if (!Process::RegisterParametersAndForcing(this, processSettings.type)) {
        throw InvalidArgument(wxString::Format(_("Fail to register the parameters and forcing for the process '%s'."),
                                               processSettings.type));
    }
}

void SettingsModel::AddProcessParameter(const string& name, float value, const string& type) {
    wxASSERT(_selectedProcess);

    if (type != "constant") {
        throw NotImplemented();
    }

    // If the parameter already exists, replace its value
    for (auto& parameter : _selectedProcess->parameters) {
        if (parameter->GetName() == name) {
            parameter->SetValue(value);
            return;
        }
    }

    auto parameter = new Parameter(name, value);

    _selectedProcess->parameters.push_back(parameter);
}

void SettingsModel::SetProcessParameterValue(const string& name, float value, const string& type) {
    wxASSERT(_selectedProcess);

    if (type != "constant") {
        throw NotImplemented();
    }

    for (auto& parameter : _selectedProcess->parameters) {
        if (parameter->GetName() == name) {
            parameter->SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen();
}

void SettingsModel::AddProcessForcing(const string& name) {
    wxASSERT(_selectedProcess);

    if (name == "precipitation") {
        _selectedProcess->forcing.push_back(Precipitation);
    } else if (name == "pet") {
        _selectedProcess->forcing.push_back(PET);
    } else if (name == "temperature") {
        _selectedProcess->forcing.push_back(Temperature);
    } else if (name == "solar_radiation" || name == "r_solar") {
        _selectedProcess->forcing.push_back(Radiation);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void SettingsModel::AddProcessOutput(const string& target, const string& fluxType) {
    wxASSERT(_selectedProcess);

    OutputSettings outputSettings;
    outputSettings.target = target;
    outputSettings.fluxType = fluxType;
    _selectedProcess->outputs.push_back(outputSettings);
}

void SettingsModel::SetProcessOutputsAsInstantaneous() {
    wxASSERT(_selectedProcess);

    for (auto& output : _selectedProcess->outputs) {
        output.isInstantaneous = true;
    }
}

void SettingsModel::SetProcessOutputsAsStatic() {
    wxASSERT(_selectedProcess);

    for (auto& output : _selectedProcess->outputs) {
        output.isStatic = true;
    }
}

void SettingsModel::OutputProcessToSameBrick() {
    wxASSERT(_selectedBrick);
    wxASSERT(_selectedProcess);

    OutputSettings outputSettings;
    outputSettings.target = _selectedBrick->name;
    outputSettings.isInstantaneous = true;
    _selectedProcess->outputs.push_back(outputSettings);
}

void SettingsModel::AddHydroUnitSplitter(const string& name, const string& type) {
    wxASSERT(_selectedStructure);

    SplitterSettings splitter;
    splitter.name = name;
    splitter.type = type;

    _selectedStructure->hydroUnitSplitters.push_back(splitter);
    _selectedSplitter = &_selectedStructure->hydroUnitSplitters[_selectedStructure->hydroUnitSplitters.size() - 1];
}

void SettingsModel::AddSubBasinSplitter(const string& name, const string& type) {
    wxASSERT(_selectedStructure);

    SplitterSettings splitter;
    splitter.name = name;
    splitter.type = type;

    _selectedStructure->subBasinSplitters.push_back(splitter);
    _selectedSplitter = &_selectedStructure->subBasinSplitters[_selectedStructure->subBasinSplitters.size() - 1];
}

void SettingsModel::AddSplitterParameter(const string& name, float value, const string& type) {
    wxASSERT(_selectedSplitter);

    if (type != "constant") {
        throw NotImplemented();
    }

    auto parameter = new Parameter(name, value);

    _selectedSplitter->parameters.push_back(parameter);
}

void SettingsModel::SetSplitterParameterValue(const string& name, float value, const string& type) {
    wxASSERT(_selectedSplitter);

    if (type != "constant") {
        throw NotImplemented();
    }

    for (auto& parameter : _selectedSplitter->parameters) {
        if (parameter->GetName() == name) {
            parameter->SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen();
}

void SettingsModel::AddSplitterForcing(const string& name) {
    wxASSERT(_selectedSplitter);

    if (name == "precipitation") {
        _selectedSplitter->forcing.push_back(Precipitation);
    } else if (name == "temperature") {
        _selectedSplitter->forcing.push_back(Temperature);
    } else if (name == "solar_radiation" || name == "r_solar") {
        _selectedSplitter->forcing.push_back(Radiation);
    } else {
        throw InvalidArgument(_("The provided forcing is not yet supported."));
    }
}

void SettingsModel::AddSplitterOutput(const string& target, const string& fluxType) {
    wxASSERT(_selectedSplitter);

    OutputSettings outputSettings;
    outputSettings.target = target;
    outputSettings.fluxType = fluxType;
    _selectedSplitter->outputs.push_back(outputSettings);
}

void SettingsModel::AddLoggingToItem(const string& itemName) {
    wxASSERT(_selectedStructure);
    if (std::find(_selectedStructure->logItems.begin(), _selectedStructure->logItems.end(), itemName) !=
        _selectedStructure->logItems.end()) {
        return;
    }
    _selectedStructure->logItems.push_back(itemName);
}

void SettingsModel::AddLoggingToItems(std::initializer_list<const string> items) {
    wxASSERT(_selectedStructure);
    for (const auto& item : items) {
        if (std::find(_selectedStructure->logItems.begin(), _selectedStructure->logItems.end(), item) !=
            _selectedStructure->logItems.end()) {
            continue;
        }
        _selectedStructure->logItems.push_back(item);
    }
}

void SettingsModel::AddBrickLogging(const string& itemName) {
    wxASSERT(_selectedBrick);
    if (std::find(_selectedBrick->logItems.begin(), _selectedBrick->logItems.end(), itemName) !=
        _selectedBrick->logItems.end()) {
        return;
    }
    _selectedBrick->logItems.push_back(itemName);
}

void SettingsModel::AddBrickLogging(std::initializer_list<const string> items) {
    wxASSERT(_selectedBrick);
    for (const auto& item : items) {
        if (std::find(_selectedBrick->logItems.begin(), _selectedBrick->logItems.end(), item) !=
            _selectedBrick->logItems.end()) {
            continue;
        }
        _selectedBrick->logItems.push_back(item);
    }
}

void SettingsModel::AddProcessLogging(const string& itemName) {
    wxASSERT(_selectedProcess);
    if (std::find(_selectedProcess->logItems.begin(), _selectedProcess->logItems.end(), itemName) !=
        _selectedProcess->logItems.end()) {
        return;
    }
    _selectedProcess->logItems.push_back(itemName);
}

void SettingsModel::AddSplitterLogging(const string& itemName) {
    wxASSERT(_selectedSplitter);
    if (std::find(_selectedSplitter->logItems.begin(), _selectedSplitter->logItems.end(), itemName) !=
        _selectedSplitter->logItems.end()) {
        return;
    }
    _selectedSplitter->logItems.push_back(itemName);
}

void SettingsModel::GeneratePrecipitationSplitters(bool withSnow) {
    wxASSERT(_selectedStructure);

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
    wxASSERT(_selectedStructure);

    for (int brickSettingsIndex : _selectedStructure->landCoverBricks) {
        BrickSettings brickSettings = _selectedStructure->hydroUnitBricks[brickSettingsIndex];
        SelectHydroUnitSplitter("snow_splitter");
        AddSplitterOutput(brickSettings.name + "_snowpack", "snow");
        AddSurfaceComponentBrick(brickSettings.name + "_snowpack", "snowpack");
        SetSurfaceComponentParent(brickSettings.name);

        AddBrickProcess("melt", snowMeltProcess, brickSettings.name);
    }
}

void SettingsModel::AddSnowIceTransformation(const string& transformationProcess) {
    wxASSERT(_selectedStructure);

    for (int brickSettingsIndex : _selectedStructure->landCoverBricks) {
        BrickSettings brickSettings = _selectedStructure->hydroUnitBricks[brickSettingsIndex];
        SelectHydroUnitBrickByName(brickSettings.name + "_snowpack");

        if (brickSettings.type == "glacier") {
            AddBrickProcess("snow_ice_transfo", transformationProcess, brickSettings.name + ":ice");
        }
    }
}

void SettingsModel::AddSnowRedistribution(const string& redistributionProcess, bool skipGlaciers) {
    wxASSERT(_selectedStructure);

    for (int brickSettingsIndex : _selectedStructure->landCoverBricks) {
        BrickSettings brickSettings = _selectedStructure->hydroUnitBricks[brickSettingsIndex];
        if (skipGlaciers && brickSettings.type == "glacier") {
            continue; // Skip glaciers for redistribution
        }
        SelectHydroUnitBrickByName(brickSettings.name + "_snowpack");
        AddBrickProcess("snow_redistribution", redistributionProcess, "lateral:snow");
    }
}

void SettingsModel::GenerateSnowpacksWithWaterRetention(const string& snowMeltProcess, const string& outflowProcess) {
    wxASSERT(_selectedStructure);

    for (int brickSettingsIndex : _selectedStructure->landCoverBricks) {
        BrickSettings brickSettings = _selectedStructure->hydroUnitBricks[brickSettingsIndex];
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
    for (auto& modelStructure : _modelStructures) {
        if (modelStructure.id == id) {
            _selectedStructure = &modelStructure;
            if (_selectedStructure->hydroUnitBricks.empty()) {
                _selectedBrick = nullptr;
            } else {
                _selectedBrick = &_selectedStructure->hydroUnitBricks[0];
            }
            return true;
        }
    }

    return false;
}

void SettingsModel::SelectHydroUnitBrick(int index) {
    wxASSERT(_selectedStructure);
    wxASSERT(_modelStructures.size() == 1);

    _selectedBrick = &_selectedStructure->hydroUnitBricks[index];
    _selectedProcess = nullptr;
}

void SettingsModel::SelectSubBasinBrick(int index) {
    wxASSERT(_selectedStructure);
    wxASSERT(_modelStructures.size() == 1);

    _selectedBrick = &_selectedStructure->subBasinBricks[index];
    _selectedProcess = nullptr;
}

bool SettingsModel::SelectHydroUnitBrickIfFound(const string& name) {
    wxASSERT(_selectedStructure);
    for (auto& brick : _selectedStructure->hydroUnitBricks) {
        if (brick.name == name) {
            _selectedBrick = &brick;
            _selectedProcess = nullptr;
            return true;
        }
    }

    return false;
}

bool SettingsModel::SelectSubBasinBrickIfFound(const string& name) {
    wxASSERT(_selectedStructure);
    for (auto& brick : _selectedStructure->subBasinBricks) {
        if (brick.name == name) {
            _selectedBrick = &brick;
            _selectedProcess = nullptr;
            return true;
        }
    }

    return false;
}

void SettingsModel::SelectHydroUnitBrick(const string& name) {
    if (!SelectHydroUnitBrickIfFound(name)) {
        throw NotFound(wxString::Format("The hydro unit brick '%s' was not found", name));
    }
    _selectedProcess = nullptr;
}

void SettingsModel::SelectHydroUnitBrickByName(const string& name) {
    SelectHydroUnitBrick(name);
}

void SettingsModel::SelectSubBasinBrick(const string& name) {
    if (!SelectSubBasinBrickIfFound(name)) {
        throw NotFound(wxString::Format("The sub-basin brick '%s' was not found", name));
    }
    _selectedProcess = nullptr;
}

void SettingsModel::SelectProcess(int index) {
    wxASSERT(_selectedBrick);
    wxASSERT(!_selectedBrick->processes.empty());

    _selectedProcess = &_selectedBrick->processes[index];
}

void SettingsModel::SelectProcess(const string& name) {
    wxASSERT(_selectedBrick);
    for (auto& process : _selectedBrick->processes) {
        if (process.name == name) {
            _selectedProcess = &process;
            return;
        }
    }

    throw InvalidArgument(wxString::Format(_("The process '%s' was not found."), name));
}

void SettingsModel::SelectProcessWithParameter(const string& name) {
    wxASSERT(_selectedBrick);
    for (auto& process : _selectedBrick->processes) {
        for (auto& parameter : process.parameters) {
            if (parameter->GetName() == name) {
                _selectedProcess = &process;
                return;
            }
        }
    }

    throw InvalidArgument(wxString::Format(_("The parameter '%s' was not found."), name));
}

void SettingsModel::SelectHydroUnitSplitter(int index) {
    wxASSERT(_selectedStructure);
    wxASSERT(_modelStructures.size() == 1);

    _selectedSplitter = &_selectedStructure->hydroUnitSplitters[index];
}

void SettingsModel::SelectSubBasinSplitter(int index) {
    wxASSERT(_selectedStructure);
    wxASSERT(_modelStructures.size() == 1);

    _selectedSplitter = &_selectedStructure->subBasinSplitters[index];
}

bool SettingsModel::SelectHydroUnitSplitterIfFound(const string& name) {
    wxASSERT(_selectedStructure);
    for (auto& splitter : _selectedStructure->hydroUnitSplitters) {
        if (splitter.name == name) {
            _selectedSplitter = &splitter;
            return true;
        }
    }

    return false;
}

bool SettingsModel::SelectSubBasinSplitterIfFound(const string& name) {
    wxASSERT(_selectedStructure);
    for (auto& splitter : _selectedStructure->subBasinSplitters) {
        if (splitter.name == name) {
            _selectedSplitter = &splitter;
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
    wxASSERT(_selectedStructure);
    wxASSERT(_modelStructures.size() == 1);

    vecStr logNames;

    for (auto& modelStructure : _modelStructures) {
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
    wxASSERT(_selectedStructure);
    vecStr names;
    for (int index : _selectedStructure->landCoverBricks) {
        names.push_back(_selectedStructure->hydroUnitBricks[index].name);
    }

    return names;
}

vecStr SettingsModel::GetSubBasinLogLabels() {
    wxASSERT(_selectedStructure);
    wxASSERT(_modelStructures.size() == 1);

    vecStr logNames;

    for (auto& modelStructure : _modelStructures) {
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

    for (const auto& label : _selectedStructure->logItems) {
        logNames.push_back(label);
    }

    return logNames;
}

vecStr SettingsModel::GetSubBasinGenericLogLabels() {
    wxASSERT(_selectedStructure);
    wxASSERT(_modelStructures.size() == 1);

    vecStr logNames;

    for (const auto& label : _selectedStructure->logItems) {
        logNames.push_back(label);
    }

    return logNames;
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
        for (auto& brick : _selectedStructure->hydroUnitBricks) {
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
        for (auto& brick : _selectedStructure->subBasinBricks) {
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