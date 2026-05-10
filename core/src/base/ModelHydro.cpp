#include "ModelHydro.h"

#include <memory>

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

        _processor.Initialize(modelSettings.GetSolverSettings());
        if (modelSettings.LogAll()) {
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
    if (modelSettings.GetStructureCount() > 1) {
        throw NotImplemented(std::format("ModelHydro::UpdateParameters - Multiple structures ({}) not yet supported",
                                         modelSettings.GetStructureCount()));
    }

    modelSettings.SelectStructure(1);

    ModelBuilder builder(_subBasin, &_timer, &_logger);
    builder.UpdateSubBasinParameters(modelSettings);
    builder.UpdateHydroUnitsParameters(modelSettings);
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

    _logger.SaveInitialValues();

    LogMessage("Simulation starting.");

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

    LogMessage("Simulation completed.");

    return {};
}

void ModelHydro::Reset() {
    _timer.Reset();
    _logger.Reset();
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

    return {};
}
