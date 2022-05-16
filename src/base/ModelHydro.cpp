#include "ModelHydro.h"

#include "FluxForcing.h"
#include "FluxToBrick.h"
#include "FluxToOutlet.h"
#include "FluxToAtmosphere.h"

ModelHydro::ModelHydro(SubBasin* subBasin)
    : m_subBasin(subBasin)
{
    m_processor.SetModel(this);
}

ModelHydro::~ModelHydro() {
}

bool ModelHydro::Initialize(SettingsModel& modelSettings) {
    try {
        BuildModelStructure(modelSettings);

        m_timer.Initialize(modelSettings.GetTimerSettings());
        m_processor.Initialize(modelSettings.GetSolverSettings());
        m_logger.InitContainer(m_timer.GetTimeStepsNb(),
                               m_subBasin->GetHydroUnitsNb(),
                               modelSettings.GetAggregatedLogLabels(),
                               modelSettings.GetHydroUnitLogLabels());
        ConnectLoggerToValues(modelSettings);
    } catch (const std::exception& e) {
        wxLogError(_("An exception occurred during model initialization: %s."), e.what());
        return false;
    }

    return true;
}

void ModelHydro::BuildModelStructure(SettingsModel& modelSettings) {
    if (modelSettings.GetStructuresNb() > 1) {
        throw NotImplemented();
    }

    modelSettings.SelectStructure(1);

    for (int iUnit = 0; iUnit < m_subBasin->GetHydroUnitsNb(); ++iUnit) {
        HydroUnit* unit = m_subBasin->GetHydroUnit(iUnit);

        for (int iBrick = 0; iBrick < modelSettings.GetBricksNb(); ++iBrick) {
            modelSettings.SelectBrick(iBrick);
            BrickSettings brickSettings = modelSettings.GetBrickSettings(iBrick);

            Brick* brick = Brick::Factory(brickSettings, unit);
            brick->SetName(brickSettings.name);

            BuildForcingConnections(brickSettings, unit, brick);

            for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
                modelSettings.SelectProcess(iProcess);
                ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

                Process* process = Process::Factory(processSettings, brick);
                process->SetName(processSettings.name);

                if (processSettings.type.IsSameAs("Overflow", false)) {
                    brick->LinkOverflow(process);
                }

                BuildForcingConnections(processSettings, unit, process);
            }
        }

        for (int iSplitter = 0; iSplitter < modelSettings.GetSplittersNb(); ++iSplitter) {
            modelSettings.SelectSplitter(iSplitter);
            SplitterSettings splitterSettings = modelSettings.GetSplitterSettings(iSplitter);

            Splitter* splitter = Splitter::Factory(splitterSettings, unit);
            splitter->SetName(splitterSettings.name);

            BuildForcingConnections(splitterSettings, unit, splitter);
        }

        BuildBricksFluxes(modelSettings, unit);
        BuildSplittersFluxes(modelSettings, unit);
    }
}

void ModelHydro::BuildBricksFluxes(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetBricksNb(); ++iBrick) {
        modelSettings.SelectBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            Flux* flux;
            Brick* brick = unit->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            if (process->ToAtmosphere()) {
                flux = new FluxToAtmosphere();
                process->AttachFluxOut(flux);
                continue;
            }

            for (const auto& output: processSettings.outputs)  {
                if (output.target.IsSameAs("outlet", false)) {
                    flux = new FluxToOutlet();
                    m_subBasin->AttachOutletFlux(flux);
                } else if (unit->HasBrick(output.target)) {
                    Brick* brickIn = unit->GetBrick(output.target);
                    flux = new FluxToBrick(brickIn);
                    brickIn->AttachFluxIn(flux);
                } else if (unit->HasSplitter(output.target)) {
                    throw NotImplemented();
                } else {
                    throw ConceptionIssue(wxString::Format(_("The target %s to attach the flux was no found"), output.target));
                }

                process->AttachFluxOut(flux);
            }
        }
    }
}

void ModelHydro::BuildSplittersFluxes(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iSplitter = 0; iSplitter < modelSettings.GetSplittersNb(); ++iSplitter) {
        modelSettings.SelectSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetSplitterSettings(iSplitter);

        Splitter* splitter = unit->GetSplitter(iSplitter);

        for (const auto& output: splitterSettings.outputs)  {
            Flux* flux;
            if (output.target.IsSameAs("outlet", false)) {
                flux = new FluxToOutlet();
                m_subBasin->AttachOutletFlux(flux);
            } else if (unit->HasBrick(output.target)) {
                Brick* brickIn = unit->GetBrick(output.target);
                flux = new FluxToBrick(brickIn);
                flux->SetAsStatic();
                brickIn->AttachFluxIn(flux);
            } else if (unit->HasSplitter(output.target)) {
                throw NotImplemented();
            } else {
                throw ConceptionIssue(wxString::Format(_("The target %s to attach the flux was no found"), output.target));
            }

            splitter->AttachFluxOut(flux);
        }
    }
}

void ModelHydro::BuildForcingConnections(BrickSettings& brickSettings, HydroUnit* unit, Brick* brick) {
    for (auto forcingType : brickSettings.forcing) {
        if (!unit->HasForcing(forcingType)) {
            auto newForcing = new Forcing(forcingType);
            unit->AddForcing(newForcing);
        }

        auto forcing = unit->GetForcing(forcingType);
        auto forcingFlux = new FluxForcing();
        forcingFlux->AttachForcing(forcing);
        brick->AttachFluxIn(forcingFlux);
    }
}

void ModelHydro::BuildForcingConnections(ProcessSettings &processSettings, HydroUnit* unit, Process* process) {
    for (auto forcingType : processSettings.forcing) {
        if (!unit->HasForcing(forcingType)) {
            auto newForcing = new Forcing(forcingType);
            unit->AddForcing(newForcing);
        }

        auto forcing = unit->GetForcing(forcingType);
        process->AttachForcing(forcing);
    }
}

void ModelHydro::BuildForcingConnections(SplitterSettings& splitterSettings, HydroUnit* unit, Splitter* splitter) {
    for (auto forcingType : splitterSettings.forcing) {
        if (!unit->HasForcing(forcingType)) {
            auto newForcing = new Forcing(forcingType);
            unit->AddForcing(newForcing);
        }

        auto forcing = unit->GetForcing(forcingType);
        splitter->AttachForcing(forcing);
    }
}

void ModelHydro::ConnectLoggerToValues(SettingsModel& modelSettings) {
    if (modelSettings.GetStructuresNb() > 1) {
        throw NotImplemented();
    }

    double* valPt = nullptr;

    // Aggregated values
    vecStr commonLogLabels = modelSettings.GetAggregatedLogLabels();
    for (int iLabel = 0; iLabel < commonLogLabels.size(); ++iLabel) {
        valPt = m_subBasin->GetValuePointer(commonLogLabels[iLabel]);
        if (valPt == nullptr) {
            throw ShouldNotHappen();
        }
        m_logger.SetAggregatedValuePointer(iLabel, valPt);
    }

    // Hydro units values
    int iLabel = 0;

    for (int iBrickType = 0; iBrickType < modelSettings.GetBricksNb(); ++iBrickType) {
        modelSettings.SelectBrick(iBrickType);
        BrickSettings brickSettings = modelSettings.GetBrickSettings(iBrickType);

        for (const auto& logItem : brickSettings.logItems) {
            for (int iUnit = 0; iUnit < m_subBasin->GetHydroUnitsNb(); ++iUnit) {
                HydroUnit* unit = m_subBasin->GetHydroUnit(iUnit);
                valPt = unit->GetBrick(iBrickType)->GetBaseValuePointer(logItem);
                if (valPt == nullptr) {
                    valPt = unit->GetBrick(iBrickType)->GetValuePointer(logItem);
                }
                if (valPt == nullptr) {
                    throw ShouldNotHappen();
                }
                m_logger.SetHydroUnitValuePointer(iUnit, iLabel, valPt);

                iLabel++;
            }
        }

        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            for (const auto& logItem : processSettings.logItems) {
                for (int iUnit = 0; iUnit < m_subBasin->GetHydroUnitsNb(); ++iUnit) {
                    HydroUnit* unit = m_subBasin->GetHydroUnit(iUnit);
                    valPt = unit->GetBrick(iBrickType)->GetProcess(iProcess)->GetValuePointer(logItem);
                    if (valPt == nullptr) {
                        throw ShouldNotHappen();
                    }
                    m_logger.SetHydroUnitValuePointer(iUnit, iLabel, valPt);

                    iLabel++;
                }
            }
        }
    }

    for (int iSplitter = 0; iSplitter < modelSettings.GetSplittersNb(); ++iSplitter) {
        modelSettings.SelectSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetSplitterSettings(iSplitter);

        for (const auto& logItem : splitterSettings.logItems) {
            for (int iUnit = 0; iUnit < m_subBasin->GetHydroUnitsNb(); ++iUnit) {
                HydroUnit* unit = m_subBasin->GetHydroUnit(iUnit);
                valPt = unit->GetSplitter(iSplitter)->GetValuePointer(logItem);
                if (valPt == nullptr) {
                    throw ShouldNotHappen();
                }
                m_logger.SetHydroUnitValuePointer(iUnit, iLabel, valPt);

                iLabel++;
            }
        }
    }
}

bool ModelHydro::IsOk() {
    if (!m_subBasin->IsOk()) return false;

    return true;
}

bool ModelHydro::Run() {
    if (!InitializeTimeSeries()) {
        return false;
    }
    while (!m_timer.IsOver()) {
        if (!m_processor.ProcessTimeStep()) {
            wxLogError(_("Failed running the model."));
            return false;
        }
        m_logger.SetDateTime(m_timer.GetDate().GetMJD());
        m_logger.Record();
        m_timer.IncrementTime();
        m_logger.Increment();
        if (!UpdateForcing()) {
            wxLogError(_("Failed updating the forcing data."));
            return false;
        }
    }
    return true;
}

bool ModelHydro::AddTimeSeries(TimeSeries* timeSeries) {
    for (auto ts: m_timeSeries) {
        if (ts->GetVariableType() == timeSeries->GetVariableType()) {
            wxLogError(_("The data variable is already linked to the model."));
            return false;
        }
    }

    if (timeSeries->GetStart().IsLaterThan(m_timer.GetStart())) {
        wxLogError(_("The data starts after the beginning of the modelling period."));
        return false;
    }

    if (timeSeries->GetEnd().IsEarlierThan(m_timer.GetEnd())) {
        wxLogError(_("The data ends before the end of the modelling period."));
        return false;
    }

    m_timeSeries.push_back(timeSeries);

    return true;
}

bool ModelHydro::AttachTimeSeriesToHydroUnits() {
    wxASSERT(m_subBasin);

    for (auto timeSeries: m_timeSeries) {
        VariableType type = timeSeries->GetVariableType();

        for (int iUnit = 0; iUnit < m_subBasin->GetHydroUnitsNb(); ++iUnit) {
            HydroUnit* unit = m_subBasin->GetHydroUnit(iUnit);
            if (unit->HasForcing(type)) {
                Forcing* forcing = unit->GetForcing(type);
                forcing->AttachTimeSeriesData(timeSeries->GetDataPointer(unit->GetId()));
            }
        }
    }

    return true;
}

bool ModelHydro::InitializeTimeSeries() {
    for (auto timeSeries: m_timeSeries) {
        if (!timeSeries->SetCursorToDate(m_timer.GetDate())) {
            return false;
        }
    }

    return true;
}

bool ModelHydro::UpdateForcing() {
    for (auto timeSeries: m_timeSeries) {
        if (!timeSeries->AdvanceOneTimeStep()) {
            return false;
        }
    }

    return true;
}
