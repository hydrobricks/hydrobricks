#include "ModelHydro.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

#include "Includes.h"
#include "ModelBuilder.h"

ModelHydro::ModelHydro(Subbasin* subbasin)
    : _subbasin(subbasin) {
    _actionsManager.SetModel(this);
    _timer.SetActionsManager(&_actionsManager);
    _timer.SetParametersUpdater(&_parametersUpdater);
}

ModelHydro::~ModelHydro() = default;

ModelResult ModelHydro::InitializeWithBasin(SettingsModel& modelSettings, SettingsBasin& basinSettings) {
    LogScope logScope(&_log);
    _network = std::make_unique<RiverNetwork>();
    if (auto r = _network->Initialize(basinSettings); !r) {
        return r;
    }
    _subbasin = _network->GetOutlet();

    // Assign each unit, and each subbasin, its structure variant from the land covers before building.
    for (Subbasin* subbasin : _network->GetProcessingOrder()) {
        ModelBuilder builder(subbasin, &_timer, &_logger);
        builder.AssignHydroUnitStructures(modelSettings, basinSettings);
        builder.AssignSubbasinStructure(modelSettings, basinSettings);
    }

    return Initialize(modelSettings, basinSettings);
}

std::vector<Subbasin*> ModelHydro::CollectSubbasins() const {
    if (_network) {
        return _network->GetProcessingOrder();
    }
    return {_subbasin};
}

ModelResult ModelHydro::Initialize(SettingsModel& modelSettings, SettingsBasin& basinSettings, bool checkProcesses) {
    LogScope logScope(&_log);
    try {
        if (!modelSettings.IsValid()) {
            return std::unexpected("Model settings are not valid.");
        }

        _subbasins = CollectSubbasins();
        for (Subbasin* subbasin : _subbasins) {
            ModelBuilder builder(subbasin, &_timer, &_logger);
            builder.BuildModelStructure(modelSettings);
        }

        if (_network) {
            _network->SetChannelRouting(modelSettings.GetChannelRoutingSettings());
        }

        _timer.Initialize(modelSettings.GetTimerSettings());

        if (!_timer.IsValid()) {
            return std::unexpected("Timer initialization failed validation.");
        }

        if (auto r = CheckTimeStepCompatibility(); !r) {
            return r;
        }

        // Convert the spin-up duration into time steps; a spin-up longer than the
        // modelling period degrades to replaying the whole period once.
        double timeStepInDays = *_timer.GetTimeStepPointer();
        _spinupSteps = static_cast<int>(modelSettings.GetTimerSettings().spinupDays / timeStepInDays);
        _spinupSteps = std::min(_spinupSteps, _timer.GetTimeStepCount());

        // One processor (and solver) per subbasin: their state vectors are independent.
        _processors.clear();
        for (Subbasin* subbasin : _subbasins) {
            auto processor = std::make_unique<Processor>();
            processor->SetSubbasin(subbasin);
            processor->Initialize(modelSettings.GetSolverSettings());
            _processors.push_back(std::move(processor));
        }

        if (modelSettings.LogAll() || modelSettings.RecordsFractions()) {
            _logger.RecordFractions();
        }
        _logger.InitContainers(_timer.GetTimeStepCount(), _subbasins, modelSettings);
        for (Subbasin* subbasin : _subbasins) {
            if (auto r = subbasin->AssignFractions(basinSettings); !r) {
                return r;
            }
            if (!subbasin->IsValid(checkProcesses)) {
                return std::unexpected(
                    std::format("Subbasin {} failed validation after initialization.", subbasin->GetId()));
            }
        }

        // The logger indexes the subbasins in processing order and the hydro units globally, in that order.
        int unitOffset = 0;
        for (int i = 0; i < static_cast<int>(_subbasins.size()); ++i) {
            ModelBuilder builder(_subbasins[i], &_timer, &_logger, i, unitOffset);
            builder.ConnectLoggerToValues(modelSettings);
            unitOffset += _subbasins[i]->GetHydroUnitCount();
        }
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Model initialization failed: {}", e.what()));
    }

    return {};
}

void ModelHydro::UpdateParameters(SettingsModel& modelSettings) {
    LogScope logScope(&_log);
    // Subbasin parameters come from each subbasin's structure variant; hydro-unit
    // parameters are updated per unit against each unit's variant.
    for (Subbasin* subbasin : _subbasins) {
        modelSettings.SelectStructure(subbasin->GetStructureId());
        ModelBuilder builder(subbasin, &_timer, &_logger);
        builder.UpdateSubbasinParameters(modelSettings);
        builder.UpdateHydroUnitsParameters(modelSettings);
    }

    // (Re)register the parameters carrying a time modifier (e.g. monthly canopy
    // capacity) with the updater so their values follow the calendar during the run.
    // Cleared first so a re-run (calibration) does not register them several times.
    _parametersUpdater.Reset();
    for (Parameter* parameter : modelSettings.GetParametersWithModifier()) {
        _parametersUpdater.AddParameter(parameter);
    }

    // Register the per-unit monthly overrides too (a parameter that is both spatial and
    // monthly): the updater writes each unit's own value for the month.
    for (HydroUnit* unit : GetHydroUnits()) {
        for (auto& [target, values] : unit->GetMonthlyParameterOverrides()) {
            _parametersUpdater.AddUnitMonthlyOverride(target, values);
        }
    }
}

ModelResult ModelHydro::CheckTimeStepCompatibility() {
    double timeStepInDays = *_timer.GetTimeStepPointer();
    if (timeStepInDays == 1.0) {
        return {};
    }

    // A discrete daily formulation (GR4J, GR6J) is not a continuous model sampled at a
    // time step: it works on the water volume of a step and advances its unit hydrograph
    // by one slot per step, so a shorter step silently reinterprets its parameters. Say
    // so at initialization rather than let it run and produce plausible nonsense.
    auto firstDailyOnly = [](const Brick* brick) -> const Process* {
        for (size_t i = 0; i < brick->GetProcessCount(); ++i) {
            const Process* process = brick->GetProcess(i);
            if (process != nullptr && process->RequiresDailyTimeStep()) {
                return process;
            }
        }
        return nullptr;
    };

    auto reject = [timeStepInDays](const Brick* brick, const Process* process) {
        return std::unexpected(
            std::format("The process '{}' of '{}' is a discrete daily formulation and only works on a "
                        "daily time step (the current one is {:g} day). Such a process works on the "
                        "water volume of a time step, so a shorter step reinterprets its parameters "
                        "rather than refining it. Use a daily time step, or a model whose processes "
                        "are continuous in time.",
                        process->GetName(), brick->GetName(), timeStepInDays));
    };

    for (Subbasin* subbasin : _subbasins) {
        for (int iBrick = 0; iBrick < subbasin->GetBrickCount(); ++iBrick) {
            const Brick* brick = subbasin->GetBrick(iBrick);
            if (const Process* process = firstDailyOnly(brick)) {
                return reject(brick, process);
            }
        }
        for (int iUnit = 0; iUnit < subbasin->GetHydroUnitCount(); ++iUnit) {
            HydroUnit* unit = subbasin->GetHydroUnit(iUnit);
            for (int iBrick = 0; iBrick < unit->GetBrickCount(); ++iBrick) {
                const Brick* brick = unit->GetBrick(iBrick);
                if (const Process* process = firstDailyOnly(brick)) {
                    return reject(brick, process);
                }
            }
        }
    }

    return {};
}

bool ModelHydro::IsValid() const {
    if (_subbasins.empty()) {
        return _subbasin != nullptr && _subbasin->IsValid();
    }
    for (Subbasin* subbasin : _subbasins) {
        if (!subbasin->IsValid()) return false;
    }

    return true;
}

void ModelHydro::Validate() const {
    for (Subbasin* subbasin : CollectSubbasins()) {
        subbasin->Validate();
    }
}

bool ModelHydro::ProcessTimeStep() {
    double timeStepInDays = *_timer.GetTimeStepPointer();
    for (size_t i = 0; i < _subbasins.size(); ++i) {
        Subbasin* subbasin = _subbasins[i];
        // Upstream first: the outlet volumes of the upstream subbasins are already computed for this step.
        if (_network) {
            _network->TransferInflow(subbasin, timeStepInDays);
        }
        if (!_processors[i]->ProcessTimeStep(timeStepInDays)) {
            return false;
        }
    }

    return true;
}

bool ModelHydro::ForcingLoaded() const {
    return !_timeSeries.empty();
}

ModelResult ModelHydro::Run() {
    LogScope logScope(&_log);
    if (auto r = InitializeTimeSeries(); !r) {
        return r;
    }

    if (_spinupSteps > 0) {
        if (auto r = RunSpinup(); !r) {
            return r;
        }
    }

    _logger.SaveInitialValues();

    // Apply time-modified parameters (e.g. monthly canopy capacity) for the start date
    // before the first step: the updater otherwise only fires on IncrementTime, which
    // runs after a step, so the first step would keep the scalar baseline.
    _parametersUpdater.DateUpdate(_timer.GetDate());

    // Per-run lifecycle messages are debug-level: at Message level they would
    // flood the output during calibration (thousands of runs).
    LogDebug("Simulation starting.");

    while (!_timer.IsOver()) {
        if (!ProcessTimeStep()) {
            return std::unexpected("Time step processing failed.");
        }
        _logger.SetDate(_timer.GetDate());
        _logger.Record();
        _timer.IncrementTime();
        _logger.Increment();
        if (auto r = UpdateForcing(); !r) {
            return std::unexpected(std::format("Failed updating forcing: {}", r.error()));
        }
    }

    LogDebug("Simulation completed.");

    return {};
}

ModelResult ModelHydro::RunSpinup() {
    LogDebug("Spin-up starting ({} time steps).", _spinupSteps);

    // Actions and date-driven parameter updates must not fire during the spin-up: the
    // same dates are replayed in the real run, and some of their side effects (e.g.
    // ice redistribution by glacier evolution) cannot be rolled back. Detach them from
    // the timer for the spin-up phase.
    _timer.SetActionsManager(nullptr);
    _timer.SetParametersUpdater(nullptr);

    ModelResult result{};
    for (int i = 0; i < _spinupSteps; ++i) {
        if (!ProcessTimeStep()) {
            result = std::unexpected("Time step processing failed during spin-up.");
            break;
        }
        _timer.IncrementTime();
        if (auto r = UpdateForcing(); !r) {
            result = std::unexpected(std::format("Failed updating forcing during spin-up: {}", r.error()));
            break;
        }
    }

    _timer.SetActionsManager(&_actionsManager);
    _timer.SetParametersUpdater(&_parametersUpdater);

    if (!result) {
        return result;
    }

    return RewindAfterSpinup();
}

ModelResult ModelHydro::RewindAfterSpinup() {
    // Restart the clock and the forcing cursors at the period start, keeping the
    // warmed-up storage states. The land cover area fractions are restored to their
    // initial values so the real run starts from the declared extents.
    _timer.Reset();
    for (Subbasin* subbasin : _subbasins) {
        subbasin->RestoreInitialAreaFractions();
    }
    if (auto r = InitializeTimeSeries(); !r) {
        return r;
    }

    LogDebug("Spin-up completed; simulation restarting at the period start.");

    return {};
}

void ModelHydro::OpenLog(const string& directory) {
    _log.Open(LogFilePath(directory));
    LogScope logScope(&_log);
    LogVersion();
}

void ModelHydro::CloseLog() {
    _log.Close();
}

void ModelHydro::Reset() {
    LogScope logScope(&_log);
    _timer.Reset();
    _logger.Reset();
    // Roll back action bookkeeping (cursors, lookup-table row, ...); the land-cover
    // extents and storages are restored by the subbasin reset below. Land covers
    // capture their initial extent on the first reset (before the first run) and
    // restore it on every later reset, so a model can be re-run (e.g. in a calibration
    // loop) from the same initial conditions even when actions changed the extents.
    _actionsManager.Reset();
    for (Subbasin* subbasin : CollectSubbasins()) {
        subbasin->Reset();
    }
    if (_network) {
        _network->Reset();
    }
}

void ModelHydro::SaveAsInitialState() {
    LogScope logScope(&_log);
    for (Subbasin* subbasin : CollectSubbasins()) {
        subbasin->SaveAsInitialState();
    }
    if (_network) {
        _network->SaveAsInitialState();
    }
}

bool ModelHydro::DumpOutputs(const string& path) {
    LogScope logScope(&_log);
    return _logger.DumpOutputs(path);
}

axd ModelHydro::GetOutletDischarge() const {
    return _logger.GetOutletDischarge();
}

axd ModelHydro::GetSubbasinDischarge(int subbasinId) const {
    return _logger.GetSubbasinDischarge(subbasinId);
}

int ModelHydro::GetSubbasinCount() const {
    return _logger.GetSubbasinCount();
}

vecInt ModelHydro::GetSubbasinIds() const {
    return _logger.GetSubbasinIds();
}

vecInt ModelHydro::GetSubbasinDownstreamIds() const {
    return _logger.GetSubbasinDownstreamIds();
}

axd ModelHydro::GetSubbasinLocalAreas() const {
    return _logger.GetSubbasinLocalAreas();
}

axd ModelHydro::GetSubbasinDrainedAreas() const {
    return _logger.GetSubbasinDrainedAreas();
}

axd ModelHydro::GetReachLengths() const {
    return GetReachProperty([](const Reach& reach) { return reach.GetLength(); });
}

axd ModelHydro::GetReachSlopes() const {
    return GetReachProperty([](const Reach& reach) { return reach.GetSlope(); });
}

axd ModelHydro::GetReachTravelTimes() const {
    return GetReachProperty([](const Reach& reach) { return reach.GetTravelTimeInDays(); });
}

axd ModelHydro::GetReachProperty(const std::function<double(const Reach&)>& get) const {
    vecInt ids = GetSubbasinIds();
    axd values = axd::Zero(static_cast<int>(ids.size()));
    if (!_network) {
        return values;
    }
    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
        Subbasin* subbasin = _network->GetSubbasinById(ids[i]);
        if (subbasin && subbasin->GetReach()) {
            values[i] = get(*subbasin->GetReach());
        }
    }

    return values;
}

vecInt ModelHydro::GetReachSubreachCounts() const {
    vecInt ids = GetSubbasinIds();
    vecInt counts(ids.size(), 1);
    if (!_network) {
        return counts;
    }
    for (size_t i = 0; i < ids.size(); ++i) {
        Subbasin* subbasin = _network->GetSubbasinById(ids[i]);
        if (subbasin && subbasin->GetReach()) {
            counts[i] = subbasin->GetReach()->GetSubreachCount();
        }
    }

    return counts;
}

string ModelHydro::GetChannelRoutingScheme() const {
    if (!_network || _network->GetSubbasinCount() == 0) {
        return "none";
    }
    Subbasin* subbasin = _network->GetSubbasin(0);
    if (!subbasin || !subbasin->GetReach()) {
        return "none";
    }

    return Reach::SchemeToString(subbasin->GetReach()->GetScheme());
}

HydroUnit* ModelHydro::GetHydroUnitById(int id) const {
    if (_network) {
        return _network->GetHydroUnitById(id);
    }
    return _subbasin ? _subbasin->GetHydroUnitById(id) : nullptr;
}

std::vector<HydroUnit*> ModelHydro::GetHydroUnits() const {
    std::vector<HydroUnit*> units;
    for (Subbasin* subbasin : CollectSubbasins()) {
        if (subbasin == nullptr) continue;
        for (int iUnit = 0; iUnit < subbasin->GetHydroUnitCount(); ++iUnit) {
            units.push_back(subbasin->GetHydroUnit(iUnit));
        }
    }
    return units;
}

double ModelHydro::GetTotalOutletDischarge() const {
    return _logger.GetTotalOutletDischarge();
}

double ModelHydro::GetTotalET() const {
    return _logger.GetTotalET();
}

double ModelHydro::GetTotalWaterStorageChanges() const {
    return _logger.GetTotalWaterStorageChanges();
}

double ModelHydro::GetTotalSnowStorageChanges() const {
    return _logger.GetTotalSnowStorageChanges();
}

double ModelHydro::GetTotalGlacierStorageChanges() const {
    return _logger.GetTotalGlacierStorageChanges();
}

axxd ModelHydro::GetHydroUnitValues(const string& label) const {
    const vecStr& labels = _logger.GetHydroUnitLabels();
    auto it = std::find(labels.begin(), labels.end(), label);
    if (it == labels.end()) {
        throw std::invalid_argument("The hydro unit component '" + label +
                                    "' was not recorded. Enable record_all to record it.");
    }
    int index = static_cast<int>(std::distance(labels.begin(), it));
    return _logger.GetHydroUnitValues()[index];
}

axxd ModelHydro::GetHydroUnitFractions(const string& label) const {
    const vecStr& labels = _logger.GetHydroUnitFractionLabels();
    auto it = std::find(labels.begin(), labels.end(), label);
    if (it == labels.end()) {
        throw std::invalid_argument("The land cover fraction '" + label +
                                    "' was not recorded. Fractions are recorded when record_all is enabled.");
    }
    int index = static_cast<int>(std::distance(labels.begin(), it));
    return _logger.GetHydroUnitFractions()[index];
}

vecStr ModelHydro::GetRecordedHydroUnitLabels() const {
    return _logger.GetHydroUnitLabels();
}

vecStr ModelHydro::GetRecordedHydroUnitFractionLabels() const {
    return _logger.GetHydroUnitFractionLabels();
}

vecInt ModelHydro::GetHydroUnitIds() const {
    return _logger.GetHydroUnitIds();
}

axd ModelHydro::GetHydroUnitAreas() const {
    return _logger.GetHydroUnitAreas();
}

string ModelHydro::GetHydroUnitLabelBasis(const string& label) const {
    const vecStr& labels = _logger.GetHydroUnitLabels();
    auto it = std::find(labels.begin(), labels.end(), label);
    if (it == labels.end()) {
        throw std::invalid_argument("The hydro unit component '" + label +
                                    "' was not recorded. Enable record_all to record it.");
    }
    int index = static_cast<int>(std::distance(labels.begin(), it));
    return _logger.GetHydroUnitLabelBases()[index];
}

axd ModelHydro::GetHydroUnitSubbasinFractions() const {
    return _logger.GetHydroUnitSubbasinFractions();
}

bool ModelHydro::AddTimeSeries(std::unique_ptr<TimeSeries> timeSeries) {
    LogScope logScope(&_log);
    // Validate time series before adding
    if (!timeSeries->IsValid()) {
        LogError("Time series is not valid and cannot be added to the model.");
        return false;
    }

    for (auto& ts : _timeSeries) {
        if (ts->GetVariableType() == timeSeries->GetVariableType()) {
            LogError("The data variable is already linked to the model.");
            return false;
        }
    }

    // The forcing is advanced one record per time step, so its spacing has to be the
    // computation step: daily data on an hourly model runs out of records a day in, and
    // hourly data on a daily model silently reads one value in twenty-four and drops the
    // rest of the water.
    double dataStep = timeSeries->GetTimeStepInDays();
    double modelStep = *_timer.GetTimeStepPointer();
    if (dataStep > 0 && std::abs(dataStep - modelStep) > 1e-9) {
        LogError(
            "The forcing is provided every {:g} day(s) but the model runs on a time step of "
            "{:g} day(s); they have to match. Provide the forcing at the resolution of the "
            "computation time step.",
            dataStep, modelStep);
        return false;
    }

    if (timeSeries->GetStart() > _timer.GetStart()) {
        LogError("The data starts after the beginning of the modelling period.");
        return false;
    }

    if (timeSeries->GetEnd() < _timer.GetEnd()) {
        LogError("The data ends before the end of the modelling period.");
        return false;
    }

    _timeSeries.push_back(std::move(timeSeries));

    return true;
}

bool ModelHydro::AddAction(Action* action) {
    LogScope logScope(&_log);
    return _actionsManager.AddAction(action);
}

int ModelHydro::GetActionCount() const {
    return _actionsManager.GetActionCount();
}

int ModelHydro::GetSporadicActionItemCount() const {
    return _actionsManager.GetSporadicActionItemCount();
}

bool ModelHydro::CreateTimeSeries(const string& varName, const axd& time, const axi& ids, const axxd& data) {
    LogScope logScope(&_log);
    try {
        auto timeSeriesPtr = TimeSeries::Create(varName, time, ids, data);
        if (!AddTimeSeries(std::move(timeSeriesPtr))) {
            return false;
        }
    } catch (const std::exception& e) {
        LogError("An exception occurred during timeseries creation: {}.", e.what());
        return false;
    }

    return true;
}

void ModelHydro::ClearTimeSeries() {
    _timeSeries.clear();  // Automatic cleanup via unique_ptr
}

bool ModelHydro::AttachTimeSeriesToHydroUnits() {
    LogScope logScope(&_log);
    assert(_subbasin);

    std::vector<HydroUnit*> units = GetHydroUnits();
    for (const auto& timeSeries : _timeSeries) {
        VariableType type = timeSeries->GetVariableType();

        for (HydroUnit* unit : units) {
            if (unit->HasForcing(type)) {
                Forcing* forcing = unit->GetForcing(type);
                forcing->AttachTimeSeriesData(timeSeries->GetDataPointer(unit->GetId()));

                // Validate forcing after attaching data
                if (!forcing->IsValid()) {
                    LogError("Forcing is not valid after attaching time series data for unit {}.", unit->GetId());
                    return false;
                }
            }
        }
    }

    return true;
}

ModelResult ModelHydro::InitializeTimeSeries() {
    for (const auto& timeSeries : _timeSeries) {
        assert(timeSeries);
        if (!timeSeries->SetCursorToDate(_timer.GetDate())) {
            return std::unexpected("Failed to position time series cursor at simulation start.");
        }
    }

    return {};
}

ModelResult ModelHydro::UpdateForcing() {
    for (const auto& timeSeries : _timeSeries) {
        if (!timeSeries->AdvanceOneTimeStep()) {
            return std::unexpected("Time series ended before simulation period.");
        }
    }

    for (Subbasin* subbasin : _subbasins) {
        subbasin->ResetForcingUpdates();
    }

    return {};
}
