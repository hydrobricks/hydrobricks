#include "SettingsModel.h"

#include <sstream>

#include "ContentTypes.h"
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

SettingsModel::~SettingsModel() = default;  // Automatic cleanup via unique_ptr

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
    assert(_selectedStructure);

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
    assert(_selectedStructure);

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
    assert(_selectedStructure);

    AddHydroUnitBrick(name, type);
    _selectedStructure->landCoverBricks.push_back(static_cast<int>(_selectedStructure->hydroUnitBricks.size() - 1));

    if (SelectHydroUnitSplitterIfFound("rain_splitter")) {
        AddSplitterOutput(name);
    }
}

void SettingsModel::AddSurfaceComponentBrick(const string& name, const string& type) {
    assert(_selectedStructure);

    AddHydroUnitBrick(name, type);
    _selectedStructure->surfaceComponentBricks.push_back(
        static_cast<int>(_selectedStructure->hydroUnitBricks.size() - 1));
}

void SettingsModel::SetSurfaceComponentParent(const string& name) {
    assert(_selectedBrick);
    _selectedBrick->parent = name;
}

void SettingsModel::AddBrickParameter(const string& name, float value, const string& type) {
    assert(_selectedBrick);

    if (type != "constant") {
        throw NotImplemented(std::format("SettingsModel::AddBrickParameter - Parameter type '{}' not supported", type));
    }

    _selectedBrick->parameters.push_back(Parameter(name, value));
}

void SettingsModel::SetBrickParameterValue(const string& name, float value, const string& type) {
    assert(_selectedBrick);

    if (type != "constant") {
        throw NotImplemented(
            std::format("SettingsModel::SetBrickParameterValue - Parameter type '{}' not supported", type));
    }

    for (auto& parameter : _selectedBrick->parameters) {
        if (parameter.GetName() == name) {
            parameter.SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen(
        std::format("SettingsModel::SetBrickParameterValue - Parameter '{}' not found after type check", name));
}

bool SettingsModel::BrickHasParameter(const string& name) {
    assert(_selectedBrick);
    for (auto& parameter : _selectedBrick->parameters) {
        if (parameter.GetName() == name) {
            return true;
        }
    }

    return false;
}

void SettingsModel::AddBrickForcing(const string& name) {
    assert(_selectedBrick);

    if (name == "precipitation") {
        _selectedBrick->forcing.push_back(Precipitation);
    } else if (name == "temperature") {
        _selectedBrick->forcing.push_back(Temperature);
    } else if (name == "solar_radiation" || name == "r_solar") {
        _selectedBrick->forcing.push_back(Radiation);
    } else {
        throw InputError(
            std::format("The provided forcing '{}' is not yet supported. Valid forcing types: precipitation, "
                        "temperature, solar_radiation (or r_solar)",
                        name));
    }
}

void SettingsModel::AddBrickProcess(const string& name, const string& type, const string& target, bool log) {
    assert(_selectedBrick);

    LogDebug("Adding brick process to brick: {}, process name: {}, process type: {}, target: {}", _selectedBrick->name,
             name, type, target);

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
            AddProcessOutput(targetSub, ContentTypeFromString(fluxType));
        } else {
            AddProcessOutput(target);
        }
    }
    if (log || _logAll) {
        // Add output logging except for processes of type "transport:" (multiple fluxes attached)
        if (type.find("transport:") == string::npos) {
            AddProcessLogging("output");
        }
    }

    // Register the related parameters
    if (!Process::RegisterParametersAndForcing(this, processSettings.type)) {
        throw ModelConfigError(
            std::format("Fail to register the parameters and forcing for the process '{}'.", processSettings.type));
    }
}

void SettingsModel::AddProcessParameter(const string& name, float value, const string& type) {
    assert(_selectedProcess);

    if (type != "constant") {
        throw NotImplemented(
            std::format("SettingsModel::AddProcessParameter - Parameter type '{}' not supported", type));
    }

    // If the parameter already exists, replace its value
    for (auto& parameter : _selectedProcess->parameters) {
        if (parameter.GetName() == name) {
            parameter.SetValue(value);
            return;
        }
    }

    _selectedProcess->parameters.push_back(Parameter(name, value));
}

void SettingsModel::SetProcessParameterValue(const string& name, float value, const string& type) {
    assert(_selectedProcess);

    if (type != "constant") {
        throw NotImplemented(
            std::format("SettingsModel::SetProcessParameterValue - Parameter type '{}' not supported", type));
    }

    for (auto& parameter : _selectedProcess->parameters) {
        if (parameter.GetName() == name) {
            parameter.SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen(
        std::format("SettingsModel::SetProcessParameterValue - Parameter '{}' not found after type check", name));
}

void SettingsModel::AddProcessForcing(const string& name) {
    assert(_selectedProcess);

    if (name == "precipitation") {
        _selectedProcess->forcing.push_back(Precipitation);
    } else if (name == "pet") {
        _selectedProcess->forcing.push_back(PET);
    } else if (name == "temperature") {
        _selectedProcess->forcing.push_back(Temperature);
    } else if (name == "solar_radiation" || name == "r_solar") {
        _selectedProcess->forcing.push_back(Radiation);
    } else {
        throw InputError(
            std::format("The provided forcing '{}' is not yet supported. Valid forcing types: precipitation, pet, "
                        "temperature, solar_radiation (or r_solar)",
                        name));
    }
}

void SettingsModel::AddProcessOutput(const string& target, ContentType fluxType) {
    assert(_selectedProcess);

    OutputSettings outputSettings;
    outputSettings.target = target;
    outputSettings.fluxType = fluxType;
    _selectedProcess->outputs.push_back(outputSettings);
}

void SettingsModel::SetProcessOutputsAsInstantaneous() {
    assert(_selectedProcess);

    for (auto& output : _selectedProcess->outputs) {
        output.isInstantaneous = true;
    }
}

void SettingsModel::SetProcessOutputsAsStatic() {
    assert(_selectedProcess);

    for (auto& output : _selectedProcess->outputs) {
        output.isStatic = true;
    }
}

void SettingsModel::OutputProcessToSameBrick() {
    assert(_selectedBrick);
    assert(_selectedProcess);

    OutputSettings outputSettings;
    outputSettings.target = _selectedBrick->name;
    outputSettings.isInstantaneous = true;
    _selectedProcess->outputs.push_back(outputSettings);
}

void SettingsModel::AddHydroUnitSplitter(const string& name, const string& type) {
    assert(_selectedStructure);

    SplitterSettings splitter;
    splitter.name = name;
    splitter.type = type;

    _selectedStructure->hydroUnitSplitters.push_back(splitter);
    _selectedSplitter = &_selectedStructure->hydroUnitSplitters[_selectedStructure->hydroUnitSplitters.size() - 1];
}

void SettingsModel::AddSubBasinSplitter(const string& name, const string& type) {
    assert(_selectedStructure);

    SplitterSettings splitter;
    splitter.name = name;
    splitter.type = type;

    _selectedStructure->subBasinSplitters.push_back(splitter);
    _selectedSplitter = &_selectedStructure->subBasinSplitters[_selectedStructure->subBasinSplitters.size() - 1];
}

void SettingsModel::AddSplitterParameter(const string& name, float value, const string& type) {
    assert(_selectedSplitter);

    if (type != "constant") {
        throw NotImplemented(
            std::format("SettingsModel::AddSplitterParameter - Parameter type '{}' not supported", type));
    }

    _selectedSplitter->parameters.push_back(Parameter(name, value));
}

void SettingsModel::SetSplitterParameterValue(const string& name, float value, const string& type) {
    assert(_selectedSplitter);

    if (type != "constant") {
        throw NotImplemented(
            std::format("SettingsModel::SetSplitterParameterValue - Parameter type '{}' not supported", type));
    }

    for (auto& parameter : _selectedSplitter->parameters) {
        if (parameter.GetName() == name) {
            parameter.SetValue(value);
            return;
        }
    }

    throw ShouldNotHappen(
        std::format("SettingsModel::SetSplitterParameterValue - Parameter '{}' not found after type check", name));
}

void SettingsModel::AddSplitterForcing(const string& name) {
    assert(_selectedSplitter);

    if (name == "precipitation") {
        _selectedSplitter->forcing.push_back(Precipitation);
    } else if (name == "temperature") {
        _selectedSplitter->forcing.push_back(Temperature);
    } else if (name == "solar_radiation" || name == "r_solar") {
        _selectedSplitter->forcing.push_back(Radiation);
    } else {
        throw InputError(
            std::format("The provided forcing '{}' is not yet supported. Valid forcing types: precipitation, "
                        "temperature, solar_radiation (or r_solar)",
                        name));
    }
}

void SettingsModel::AddSplitterOutput(const string& target, const ContentType fluxType) {
    assert(_selectedSplitter);

    OutputSettings outputSettings;
    outputSettings.target = target;
    outputSettings.fluxType = fluxType;
    _selectedSplitter->outputs.push_back(outputSettings);
}

void SettingsModel::AddLoggingToItem(const string& itemName) {
    assert(_selectedStructure);
    if (std::find(_selectedStructure->logItems.begin(), _selectedStructure->logItems.end(), itemName) !=
        _selectedStructure->logItems.end()) {
        return;
    }
    _selectedStructure->logItems.push_back(itemName);
}

void SettingsModel::AddLoggingToItems(std::initializer_list<const string> items) {
    assert(_selectedStructure);
    for (const auto& item : items) {
        if (std::find(_selectedStructure->logItems.begin(), _selectedStructure->logItems.end(), item) !=
            _selectedStructure->logItems.end()) {
            continue;
        }
        _selectedStructure->logItems.push_back(item);
    }
}

void SettingsModel::AddBrickLogging(const string& itemName) {
    assert(_selectedBrick);
    if (std::find(_selectedBrick->logItems.begin(), _selectedBrick->logItems.end(), itemName) !=
        _selectedBrick->logItems.end()) {
        return;
    }
    _selectedBrick->logItems.push_back(itemName);
}

void SettingsModel::AddBrickLogging(std::initializer_list<const string> items) {
    assert(_selectedBrick);
    for (const auto& item : items) {
        if (std::find(_selectedBrick->logItems.begin(), _selectedBrick->logItems.end(), item) !=
            _selectedBrick->logItems.end()) {
            continue;
        }
        _selectedBrick->logItems.push_back(item);
    }
}

void SettingsModel::AddProcessLogging(const string& itemName) {
    assert(_selectedProcess);
    if (std::find(_selectedProcess->logItems.begin(), _selectedProcess->logItems.end(), itemName) !=
        _selectedProcess->logItems.end()) {
        return;
    }
    _selectedProcess->logItems.push_back(itemName);
}

void SettingsModel::AddSplitterLogging(const string& itemName) {
    assert(_selectedSplitter);
    if (std::find(_selectedSplitter->logItems.begin(), _selectedSplitter->logItems.end(), itemName) !=
        _selectedSplitter->logItems.end()) {
        return;
    }
    _selectedSplitter->logItems.push_back(itemName);
}

void SettingsModel::GeneratePrecipitationSplitters(bool withSnow) {
    assert(_selectedStructure);

    if (withSnow) {
        // Rain/snow splitter
        AddHydroUnitSplitter("snow_rain_transition", "snow_rain");
        AddSplitterForcing("precipitation");
        AddSplitterForcing("temperature");
        AddSplitterOutput("rain_splitter");
        AddSplitterOutput("snow_splitter", ContentType::Snow);
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
    assert(_selectedStructure);

    for (int brickSettingsIndex : _selectedStructure->landCoverBricks) {
        const BrickSettings& brickSettings = _selectedStructure->hydroUnitBricks[brickSettingsIndex];
        string brickName = brickSettings.name;  // Store the name before potential reallocation
        SelectHydroUnitSplitter("snow_splitter");
        AddSplitterOutput(brickName + "_snowpack", ContentType::Snow);
        AddSurfaceComponentBrick(brickName + "_snowpack", "snowpack");
        SetSurfaceComponentParent(brickName);

        AddBrickProcess("melt", snowMeltProcess, brickName);
    }
}

void SettingsModel::AddSnowIceTransformation(const string& transformationProcess) {
    assert(_selectedStructure);

    for (int brickSettingsIndex : _selectedStructure->landCoverBricks) {
        const BrickSettings& brickSettings = _selectedStructure->hydroUnitBricks[brickSettingsIndex];
        SelectHydroUnitBrickByName(brickSettings.name + "_snowpack");

        if (brickSettings.type == "glacier") {
            AddBrickProcess("snow_ice_transfo", transformationProcess, brickSettings.name + ":ice");
        }
    }
}

void SettingsModel::AddSnowRedistribution(const string& redistributionProcess, bool skipGlaciers) {
    assert(_selectedStructure);

    for (int brickSettingsIndex : _selectedStructure->landCoverBricks) {
        const BrickSettings& brickSettings = _selectedStructure->hydroUnitBricks[brickSettingsIndex];
        if (skipGlaciers && brickSettings.type == "glacier") {
            continue;  // Skip glaciers for redistribution
        }
        SelectHydroUnitBrickByName(brickSettings.name + "_snowpack");
        AddBrickProcess("snow_redistribution", redistributionProcess, "lateral:snow");
        SetProcessOutputsAsInstantaneous();
    }
}

void SettingsModel::GenerateSnowpacksWithWaterRetention(const string& snowMeltProcess, const string& outflowProcess) {
    assert(_selectedStructure);

    for (int brickSettingsIndex : _selectedStructure->landCoverBricks) {
        const BrickSettings& brickSettings = _selectedStructure->hydroUnitBricks[brickSettingsIndex];
        string brickName = brickSettings.name;  // Store the name before potential reallocation
        SelectHydroUnitSplitter("snow_splitter");
        AddSplitterOutput(brickName + "_snowpack", ContentType::Snow);
        AddSurfaceComponentBrick(brickName + "_snowpack", "snowpack");
        SetSurfaceComponentParent(brickName);

        AddBrickProcess("melt", snowMeltProcess);
        OutputProcessToSameBrick();

        AddBrickProcess("meltwater", outflowProcess);
        AddProcessOutput(brickName);
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
    assert(_selectedStructure);
    assert(_modelStructures.size() == 1);

    _selectedBrick = &_selectedStructure->hydroUnitBricks[index];
    _selectedProcess = nullptr;
}

void SettingsModel::SelectSubBasinBrick(int index) {
    assert(_selectedStructure);
    assert(_modelStructures.size() == 1);

    _selectedBrick = &_selectedStructure->subBasinBricks[index];
    _selectedProcess = nullptr;
}

bool SettingsModel::SelectHydroUnitBrickIfFound(const string& name) {
    assert(_selectedStructure);
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
    assert(_selectedStructure);
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
        throw ModelConfigError(std::format("The hydro unit brick '{}' was not found", name));
    }
    _selectedProcess = nullptr;
}

void SettingsModel::SelectHydroUnitBrickByName(const string& name) {
    SelectHydroUnitBrick(name);
}

void SettingsModel::SelectSubBasinBrick(const string& name) {
    if (!SelectSubBasinBrickIfFound(name)) {
        throw ModelConfigError(std::format("The sub-basin brick '{}' was not found", name));
    }
    _selectedProcess = nullptr;
}

void SettingsModel::SelectProcess(int index) {
    assert(_selectedBrick);
    assert(!_selectedBrick->processes.empty());

    _selectedProcess = &_selectedBrick->processes[index];
}

void SettingsModel::SelectProcess(const string& name) {
    assert(_selectedBrick);
    for (auto& process : _selectedBrick->processes) {
        if (process.name == name) {
            _selectedProcess = &process;
            return;
        }
    }

    throw ModelConfigError(std::format("The process '{}' was not found.", name));
}

void SettingsModel::SelectProcessWithParameter(const string& name) {
    assert(_selectedBrick);
    for (auto& process : _selectedBrick->processes) {
        for (auto& parameter : process.parameters) {
            if (parameter.GetName() == name) {
                _selectedProcess = &process;
                return;
            }
        }
    }

    throw ModelConfigError(std::format("The parameter '{}' was not found.", name));
}

void SettingsModel::SelectHydroUnitSplitter(int index) {
    assert(_selectedStructure);
    assert(_modelStructures.size() == 1);

    _selectedSplitter = &_selectedStructure->hydroUnitSplitters[index];
}

void SettingsModel::SelectSubBasinSplitter(int index) {
    assert(_selectedStructure);
    assert(_modelStructures.size() == 1);

    _selectedSplitter = &_selectedStructure->subBasinSplitters[index];
}

bool SettingsModel::SelectHydroUnitSplitterIfFound(const string& name) {
    assert(_selectedStructure);
    for (auto& splitter : _selectedStructure->hydroUnitSplitters) {
        if (splitter.name == name) {
            _selectedSplitter = &splitter;
            return true;
        }
    }

    return false;
}

bool SettingsModel::SelectSubBasinSplitterIfFound(const string& name) {
    assert(_selectedStructure);
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
        throw ModelConfigError(std::format("The hydro unit splitter '{}' was not found", name));
    }
}

void SettingsModel::SelectSubBasinSplitter(const string& name) {
    if (!SelectSubBasinSplitterIfFound(name)) {
        throw ModelConfigError(std::format("The sub-basin splitter '{}' was not found", name));
    }
}

vecStr SettingsModel::GetHydroUnitLogLabels() const {
    assert(_selectedStructure);
    assert(_modelStructures.size() == 1);

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
    assert(_selectedStructure);
    vecStr names;
    for (int index : _selectedStructure->landCoverBricks) {
        names.push_back(_selectedStructure->hydroUnitBricks[index].name);
    }

    return names;
}

vecStr SettingsModel::GetSubBasinLogLabels() const {
    assert(_selectedStructure);
    assert(_modelStructures.size() == 1);

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

vecStr SettingsModel::GetSubBasinGenericLogLabels() const {
    assert(_selectedStructure);
    assert(_modelStructures.size() == 1);

    vecStr logNames;

    for (const auto& label : _selectedStructure->logItems) {
        logNames.push_back(label);
    }

    return logNames;
}

bool SettingsModel::SetParameterValue(const string& component, const string& name, float value) {
    // Check if the parameter should be set for multiple components
    if (component.find(',') != string::npos) {
        vecStr components;
        {
            std::istringstream _ss(component);
            string _tok;
            while (std::getline(_ss, _tok, ',')) {
                _tok.erase(0, _tok.find_first_not_of(" "));
                _tok.erase(_tok.find_last_not_of(" ") + 1);
                if (!_tok.empty()) components.push_back(_tok);
            }
        }
        for (const auto& componentItem : components) {
            if (!SetParameterValue(componentItem, name, value)) {
                LogError("Fail to set the parameter '{}' for the component '{}'.", name, componentItem);
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
        string type = component.substr(component.find(':') + 1);

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
        LogError("Cannot find the component '{}'.", component);
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

bool SettingsModel::IsValid() const {
    // Check that solver is configured
    if (_solver.name.empty()) {
        LogError("SettingsModel: Solver not configured.");
        return false;
    }

    // Check that timer is configured
    if (_timer.start.empty() || _timer.end.empty()) {
        LogError("SettingsModel: Timer start or end date not set.");
        return false;
    }

    if (_timer.timeStep <= 0) {
        LogError("SettingsModel: Time step must be positive.");
        return false;
    }

    // Check that at least one structure exists
    if (_modelStructures.empty()) {
        LogError("SettingsModel: No structures defined.");
        return false;
    }

    return true;
}

void SettingsModel::Validate() const {
    if (!IsValid()) {
        string msg = std::format("SettingsModel validation failed. Solver: '%s', Start: '%s', End: '%s', TimeStep: %d",
                                 _solver.name, _timer.start, _timer.end, _timer.timeStep);
        throw ModelConfigError(msg);
    }
}
