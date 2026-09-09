#include "ModelHydro.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

#include "Includes.h"
#include "ModelBuilder.h"

ModelHydro::ModelHydro(SubBasin* subBasin)
    : _subBasin(subBasin) {
    _processor.SetModel(this);
    _actionsManager.SetModel(this);
    _timer.SetActionsManager(&_actionsManager);
    _timer.SetParametersUpdater(&_parametersUpdater);
}

ModelHydro::~ModelHydro() = default;

ModelResult ModelHydro::InitializeWithBasin(SettingsModel& modelSettings, SettingsBasin& basinSettings) {
    _ownedSubBasin = std::make_unique<SubBasin>();
    _subBasin = _ownedSubBasin.get();
    if (auto r = _subBasin->Initialize(basinSettings); !r) {
        return r;
    }

    // Assign each unit its structure variant from its land covers before building.
    ModelBuilder builder(_subBasin, &_timer, &_logger);
    builder.AssignHydroUnitStructures(modelSettings, basinSettings);

    return Initialize(modelSettings, basinSettings);
}

ModelResult ModelHydro::Initialize(SettingsModel& modelSettings, SettingsBasin& basinSettings, bool checkProcesses) {
    try {
        if (!modelSettings.IsValid()) {
            return std::unexpected("Model settings are not valid.");
        }

        ModelBuilder builder(_subBasin, &_timer, &_logger);
        builder.BuildModelStructure(modelSettings);

        _timer.Initialize(modelSettings.GetTimerSettings());

        if (!_timer.IsValid()) {
            return std::unexpected("Timer initialization failed validation.");
        }

        // Convert the spin-up duration into time steps; a spin-up longer than the
        // modelling period degrades to replaying the whole period once.
        double timeStepInDays = *_timer.GetTimeStepPointer();
        _spinupSteps = static_cast<int>(modelSettings.GetTimerSettings().spinupDays / timeStepInDays);
        _spinupSteps = std::min(_spinupSteps, _timer.GetTimeStepCount());

        _processor.Initialize(modelSettings.GetSolverSettings());
        if (modelSettings.LogAll() || modelSettings.RecordsFractions()) {
            _logger.RecordFractions();
        }
        _logger.InitContainers(_timer.GetTimeStepCount(), _subBasin, modelSettings);
        if (auto r = _subBasin->AssignFractions(basinSettings); !r) {
            return r;
        }

        if (!_subBasin->IsValid(checkProcesses)) {
            return std::unexpected("Sub-basin failed validation after initialization.");
        }

        builder.ConnectLoggerToValues(modelSettings);
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Model initialization failed: {}", e.what()));
    }

    return {};
}

void ModelHydro::UpdateParameters(SettingsModel& modelSettings) {
    // Sub-basin parameters come from the primary structure (1); hydro-unit
    // parameters are updated per unit against each unit's structure variant.
    modelSettings.SelectStructure(1);

    ModelBuilder builder(_subBasin, &_timer, &_logger);
    builder.UpdateSubBasinParameters(modelSettings);
    builder.UpdateHydroUnitsParameters(modelSettings);

    // (Re)register the parameters carrying a time modifier (e.g. monthly canopy
    // capacity) with the updater so their values follow the calendar during the run.
    // Cleared first so a re-run (calibration) does not register them several times.
    _parametersUpdater.Reset();
    for (Parameter* parameter : modelSettings.GetParametersWithModifier()) {
        _parametersUpdater.AddParameter(parameter);
    }
}

bool ModelHydro::IsValid() const {
    if (!_subBasin->IsValid()) return false;

    return true;
}

void ModelHydro::Validate() const {
    _subBasin->Validate();
}

bool ModelHydro::ForcingLoaded() const {
    return !_timeSeries.empty();
}

ModelResult ModelHydro::Run() {
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
        if (!_processor.ProcessTimeStep(*_timer.GetTimeStepPointer())) {
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
        if (!_processor.ProcessTimeStep(*_timer.GetTimeStepPointer())) {
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
    _subBasin->RestoreInitialAreaFractions();
    if (auto r = InitializeTimeSeries(); !r) {
        return r;
    }

    LogDebug("Spin-up completed; simulation restarting at the period start.");

    return {};
}

void ModelHydro::Reset() {
    _timer.Reset();
    _logger.Reset();
    // Roll back action bookkeeping (cursors, lookup-table row, ...); the land-cover
    // extents and storages are restored by the sub-basin reset below. Land covers
    // capture their initial extent on the first reset (before the first run) and
    // restore it on every later reset, so a model can be re-run (e.g. in a calibration
    // loop) from the same initial conditions even when actions changed the extents.
    _actionsManager.Reset();
    _subBasin->Reset();
}

void ModelHydro::SaveAsInitialState() {
    _subBasin->SaveAsInitialState();
}

bool ModelHydro::DumpOutputs(const string& path) {
    return _logger.DumpOutputs(path);
}

axd ModelHydro::GetOutletDischarge() const {
    return _logger.GetOutletDischarge();
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

bool ModelHydro::AddTimeSeries(std::unique_ptr<TimeSeries> timeSeries) {
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
    return _actionsManager.AddAction(action);
}

int ModelHydro::GetActionCount() const {
    return _actionsManager.GetActionCount();
}

int ModelHydro::GetSporadicActionItemCount() const {
    return _actionsManager.GetSporadicActionItemCount();
}

bool ModelHydro::CreateTimeSeries(const string& varName, const axd& time, const axi& ids, const axxd& data) {
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
    assert(_subBasin);

    for (const auto& timeSeries : _timeSeries) {
        VariableType type = timeSeries->GetVariableType();

        for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitCount(); ++iUnit) {
            HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);
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

    _subBasin->ResetForcingUpdates();

    return {};
}
