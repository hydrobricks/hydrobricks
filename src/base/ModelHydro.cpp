#include "ModelHydro.h"

#include "Includes.h"
#include "FluxForcing.h"
#include "FluxSimple.h"
#include "FluxToAtmosphere.h"
#include "FluxToBrick.h"
#include "FluxToOutlet.h"
#include "SurfaceComponent.h"
#include "FluxToBrickInstantaneous.h"

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
        g_timeStepInDays = *m_timer.GetTimeStepPointer();
        m_processor.Initialize(modelSettings.GetSolverSettings());
        m_logger.InitContainer(m_timer.GetTimeStepsNb(),
                               m_subBasin->GetHydroUnitsIds(),
                               modelSettings.GetSubBasinLogLabels(),
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

    CreateSubBasinComponents(modelSettings);
    CreateHydroUnitsComponents(modelSettings);
}

void ModelHydro::CreateSubBasinComponents(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBricksNb(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        BrickSettings brickSettings = modelSettings.GetSubBasinBrickSettings(iBrick);

        // Create the brick
        Brick* brick = Brick::Factory(brickSettings);
        brick->SetName(brickSettings.name);
        brick->AssignParameters(brickSettings);
        m_subBasin->AddBrick(brick);

        // Create the processes
        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            Process* process = Process::Factory(processSettings, brick);
            process->SetName(processSettings.name);
            brick->AddProcess(process);

            if (processSettings.type.IsSameAs("Overflow", false)) {
                brick->GetWaterContainer()->LinkOverflow(process);
            }
        }
    }

    // Create the splitters
    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplittersNb(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);

        Splitter* splitter = Splitter::Factory(splitterSettings);
        splitter->SetName(splitterSettings.name);
        m_subBasin->AddSplitter(splitter);
    }

    LinkSubBasinProcessesTargetBricks(modelSettings);
    BuildSubBasinBricksFluxes(modelSettings);
    BuildSubBasinSplittersFluxes(modelSettings);
}

void ModelHydro::CreateHydroUnitsComponents(SettingsModel& modelSettings) {
    for (int iUnit = 0; iUnit < m_subBasin->GetHydroUnitsNb(); ++iUnit) {
        HydroUnit* unit = m_subBasin->GetHydroUnit(iUnit);

        // Create the bricks for the hydro unit
        for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBricksNb(); ++iBrick) {
            modelSettings.SelectHydroUnitBrick(iBrick);
            BrickSettings brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrick);

            Brick* brick = Brick::Factory(brickSettings);
            brick->SetName(brickSettings.name);
            brick->AssignParameters(brickSettings);
            unit->AddBrick(brick);

            BuildForcingConnections(brickSettings, unit, brick);

            // Create the processes
            for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
                modelSettings.SelectProcess(iProcess);
                ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

                Process* process = Process::Factory(processSettings, brick);
                process->SetName(processSettings.name);
                brick->AddProcess(process);

                if (processSettings.type.IsSameAs("Overflow", false)) {
                    brick->GetWaterContainer()->LinkOverflow(process);
                }

                BuildForcingConnections(processSettings, unit, process);
            }
        }

        // Create the splitters
        for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplittersNb(); ++iSplitter) {
            modelSettings.SelectHydroUnitSplitter(iSplitter);
            SplitterSettings splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);

            Splitter* splitter = Splitter::Factory(splitterSettings);
            splitter->SetName(splitterSettings.name);
            unit->AddSplitter(splitter);

            BuildForcingConnections(splitterSettings, unit, splitter);
        }

        LinkRelatedSurfaceBricks(modelSettings, unit);
        LinkHydroUnitProcessesTargetBricks(modelSettings, unit);
        BuildHydroUnitBricksFluxes(modelSettings, unit);
        BuildHydroUnitSplittersFluxes(modelSettings, unit);
    }
}

void ModelHydro::LinkRelatedSurfaceBricks(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBricksNb(); ++iBrick) {
        BrickSettings brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrick);
        if (!brickSettings.relatedSurfaceBricks.empty()) {
            auto brick = dynamic_cast<SurfaceComponent*>(unit->GetBrick(iBrick));
            for (const auto& relatedSurfaceBrick: brickSettings.relatedSurfaceBricks) {
                auto relatedBrick = dynamic_cast<SurfaceComponent*>(unit->GetBrick(relatedSurfaceBrick));
                brick->AddToRelatedBricks(relatedBrick);
            }
        }
    }
}

void ModelHydro::LinkSubBasinProcessesTargetBricks(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBricksNb(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = m_subBasin->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            if (process->NeedsTargetBrickLinking()) {
                if (processSettings.outputs.size() != 1) {
                    throw ConceptionIssue(_("There can only be a single process output for brick linking."));
                }
                Brick* targetBrick = m_subBasin->GetBrick(processSettings.outputs[0].target);
                process->SetTargetBrick(targetBrick);
            }
        }
    }
}

void ModelHydro::LinkHydroUnitProcessesTargetBricks(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBricksNb(); ++iBrick) {
        modelSettings.SelectHydroUnitBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = unit->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            if (process->NeedsTargetBrickLinking()) {
                if (processSettings.outputs.size() != 1) {
                    throw ConceptionIssue(_("There can only be a single process output for brick linking."));
                }
                Brick* targetBrick = nullptr;
                if (unit->HasBrick(processSettings.outputs[0].target)) {
                    targetBrick = unit->GetBrick(processSettings.outputs[0].target);
                } else {
                    targetBrick = m_subBasin->GetBrick(processSettings.outputs[0].target);
                }
                process->SetTargetBrick(targetBrick);
            }
        }
    }
}

void ModelHydro::BuildSubBasinBricksFluxes(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBricksNb(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            Flux* flux;
            Brick* brick = m_subBasin->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            // Water goes to the atmosphere (ET)
            if (process->ToAtmosphere()) {
                flux = new FluxToAtmosphere();
                process->AttachFluxOut(flux);
                continue;
            }

            for (const auto& output: processSettings.outputs)  {
                if (output.target.IsSameAs("outlet", false)) {

                    // Water goes to the outlet
                    flux = new FluxToOutlet();
                    flux->SetType(output.fluxType);
                    m_subBasin->AttachOutletFlux(flux);

                } else if (m_subBasin->HasBrick(output.target)) {

                    // Water goes to another brick
                    Brick* targetBrick = m_subBasin->GetBrick(output.target);
                    if (output.instantaneous) {
                        flux = new FluxToBrickInstantaneous(targetBrick);
                        flux->SetType(output.fluxType);
                        targetBrick->AttachFluxIn(flux);
                    } else {
                        flux = new FluxToBrick(targetBrick);
                        flux->SetType(output.fluxType);
                        targetBrick->AttachFluxIn(flux);
                    }

                } else if (m_subBasin->HasSplitter(output.target)) {

                    // Water goes to a splitter
                    Splitter* targetSplitter = m_subBasin->GetSplitter(output.target);
                    flux = new FluxSimple();
                    flux->SetType(output.fluxType);
                    flux->SetAsStatic();
                    targetSplitter->AttachFluxIn(flux);

                } else {
                    throw ConceptionIssue(wxString::Format(_("The target %s to attach the flux was no found"), output.target));
                }

                process->AttachFluxOut(flux);
            }
        }
    }
}

void ModelHydro::BuildHydroUnitBricksFluxes(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBricksNb(); ++iBrick) {
        modelSettings.SelectHydroUnitBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            Flux* flux;
            Brick* brick = unit->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            // Water goes to the atmosphere (ET)
            if (process->ToAtmosphere()) {
                flux = new FluxToAtmosphere();
                process->AttachFluxOut(flux);
                continue;
            }

            for (const auto& output: processSettings.outputs)  {
                if (output.target.IsSameAs("outlet", false)) {

                    // Water goes to the outlet
                    flux = new FluxToOutlet();
                    flux->SetType(output.fluxType);

                    // From hydro unit to basin: weight by hydro unit area
                    flux->MultiplyFraction(unit->GetArea() / m_subBasin->GetArea());

                    // If surface component: weight by surface area
                    if (brick->IsSurfaceComponent()) {
                        flux->NeedsWeighting(true);
                    }
                    m_subBasin->AttachOutletFlux(flux);

                } else if (unit->HasBrick(output.target) || m_subBasin->HasBrick(output.target)) {
                    bool toSubBasin = false;
                    Brick* targetBrick = nullptr;

                    // Look for target brick
                    if (unit->HasBrick(output.target)) {
                        targetBrick = unit->GetBrick(output.target);
                    } else {
                        targetBrick = m_subBasin->GetBrick(output.target);
                        toSubBasin = true;
                    }

                    // If leaves surface components: weight by surface area
                    if (brick->IsSurfaceComponent() && !targetBrick->IsSurfaceComponent()) {
                        flux->NeedsWeighting(true);
                    }

                    // Create the flux
                    if (output.instantaneous) {
                        flux = new FluxToBrickInstantaneous(targetBrick);
                    } else {
                        flux = new FluxToBrick(targetBrick);
                    }

                    flux->SetType(output.fluxType);

                    // From hydro unit to basin: weight by hydro unit area
                    if (toSubBasin) {
                        flux->MultiplyFraction(unit->GetArea() / m_subBasin->GetArea());
                    }

                    targetBrick->AttachFluxIn(flux);

                } else if (unit->HasSplitter(output.target) || m_subBasin->HasSplitter(output.target)) {
                    bool toSubBasin = false;
                    Splitter* targetSplitter = nullptr;

                    // Look for target splitter
                    if (unit->HasSplitter(output.target)) {
                        targetSplitter = unit->GetSplitter(output.target);
                    } else {
                        targetSplitter = m_subBasin->GetSplitter(output.target);
                        toSubBasin = true;
                    }

                    // Create the flux
                    flux = new FluxSimple();
                    flux->SetType(output.fluxType);
                    flux->SetAsStatic();

                    // If coming from a surface component: weight by surface area
                    if (brick->IsSurfaceComponent()) {
                        flux->NeedsWeighting(true);
                    }

                    // From hydro unit to basin: weight by hydro unit area
                    if (toSubBasin) {
                        flux->MultiplyFraction(unit->GetArea() / m_subBasin->GetArea());
                    }

                    targetSplitter->AttachFluxIn(flux);

                } else {
                    throw ConceptionIssue(wxString::Format(_("The target %s to attach the flux was no found"), output.target));
                }

                process->AttachFluxOut(flux);
            }
        }
    }
}

void ModelHydro::BuildSubBasinSplittersFluxes(SettingsModel& modelSettings) {
    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplittersNb(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);

        Splitter* splitter = m_subBasin->GetSplitter(iSplitter);

        for (const auto& output: splitterSettings.outputs)  {
            Flux* flux;
            if (output.target.IsSameAs("outlet", false)) {

                // Water goes to the outlet
                flux = new FluxToOutlet();
                flux->SetType(output.fluxType);
                m_subBasin->AttachOutletFlux(flux);

            } else if (m_subBasin->HasBrick(output.target)) {

                // Look for target brick
                Brick* targetBrick = m_subBasin->GetBrick(output.target);
                flux = new FluxToBrick(targetBrick);
                flux->SetAsStatic();
                flux->SetType(output.fluxType);
                targetBrick->AttachFluxIn(flux);

            } else if (m_subBasin->HasSplitter(output.target)) {

                // Look for target splitter
                Splitter* targetSplitter = m_subBasin->GetSplitter(output.target);
                flux = new FluxSimple();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);
                targetSplitter->AttachFluxIn(flux);

            } else {
                throw ConceptionIssue(wxString::Format(_("The target %s to attach the flux was no found"), output.target));
            }

            splitter->AttachFluxOut(flux);
        }
    }
}

void ModelHydro::BuildHydroUnitSplittersFluxes(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplittersNb(); ++iSplitter) {
        modelSettings.SelectHydroUnitSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);

        Splitter* splitter = unit->GetSplitter(iSplitter);

        for (const auto& output: splitterSettings.outputs)  {
            Flux* flux;
            if (output.target.IsSameAs("outlet", false)) {

                // Water goes to the outlet
                flux = new FluxToOutlet();
                flux->SetType(output.fluxType);

                // From hydro unit to basin: weight by hydro unit area
                flux->MultiplyFraction(unit->GetArea() / m_subBasin->GetArea());

                m_subBasin->AttachOutletFlux(flux);

            } else if (unit->HasBrick(output.target) || m_subBasin->HasBrick(output.target)) {
                bool toSubBasin = false;
                Brick* targetBrick = nullptr;

                // Look for target brick
                if (unit->HasBrick(output.target)) {
                    targetBrick = unit->GetBrick(output.target);
                } else {
                    targetBrick = m_subBasin->GetBrick(output.target);
                    toSubBasin = true;
                }

                // Create flux
                flux = new FluxToBrick(targetBrick);
                flux->SetAsStatic();
                flux->SetType(output.fluxType);

                // From hydro unit to basin: weight by hydro unit area
                if (toSubBasin) {
                    flux->MultiplyFraction(unit->GetArea() / m_subBasin->GetArea()); // From hydro unit to basin
                }

                targetBrick->AttachFluxIn(flux);

            } else if (unit->HasSplitter(output.target) || m_subBasin->HasSplitter(output.target)) {
                bool toSubBasin = false;
                Splitter* targetSplitter = nullptr;

                // Look for the target splitter
                if (unit->HasSplitter(output.target)) {
                    targetSplitter = unit->GetSplitter(output.target);
                } else {
                    targetSplitter = m_subBasin->GetSplitter(output.target);
                    toSubBasin = true;
                }

                // Create flux
                flux = new FluxSimple();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);

                // From hydro unit to basin: weight by hydro unit area
                if (toSubBasin) {
                    flux->MultiplyFraction(unit->GetArea() / m_subBasin->GetArea()); // From hydro unit to basin
                }

                targetSplitter->AttachFluxIn(flux);

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

    // Sub basin values
    int iLabel = 0;

    for (int iBrickType = 0; iBrickType < modelSettings.GetSubBasinBricksNb(); ++iBrickType) {
        modelSettings.SelectSubBasinBrick(iBrickType);
        BrickSettings brickSettings = modelSettings.GetSubBasinBrickSettings(iBrickType);

        for (const auto& logItem : brickSettings.logItems) {
            valPt = m_subBasin->GetBrick(iBrickType)->GetBaseValuePointer(logItem);
            if (valPt == nullptr) {
                valPt = m_subBasin->GetBrick(iBrickType)->GetValuePointer(logItem);
            }
            if (valPt == nullptr) {
                throw ShouldNotHappen();
            }
            m_logger.SetSubBasinValuePointer(iLabel, valPt);
            iLabel++;
        }

        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            for (const auto& logItem : processSettings.logItems) {
                valPt = m_subBasin->GetBrick(iBrickType)->GetProcess(iProcess)->GetValuePointer(logItem);
                if (valPt == nullptr) {
                    throw ShouldNotHappen();
                }
                m_logger.SetSubBasinValuePointer(iLabel, valPt);
                iLabel++;
            }
        }
    }

    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplittersNb(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);

        for (const auto& logItem : splitterSettings.logItems) {
            valPt = m_subBasin->GetSplitter(iSplitter)->GetValuePointer(logItem);
            if (valPt == nullptr) {
                throw ShouldNotHappen();
            }
            m_logger.SetSubBasinValuePointer(iLabel, valPt);
            iLabel++;
        }
    }

    vecStr genericLogLabels = modelSettings.GetSubBasinGenericLogLabels();
    for (auto & genericLogLabel : genericLogLabels) {
        valPt = m_subBasin->GetValuePointer(genericLogLabel);
        if (valPt == nullptr) {
            throw ShouldNotHappen();
        }
        m_logger.SetSubBasinValuePointer(iLabel, valPt);
        iLabel++;
    }

    // Hydro units values
    iLabel = 0;

    for (int iBrickType = 0; iBrickType < modelSettings.GetHydroUnitBricksNb(); ++iBrickType) {
        modelSettings.SelectHydroUnitBrick(iBrickType);
        BrickSettings brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrickType);

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
            }
            iLabel++;
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
                }
                iLabel++;
            }
        }
    }

    for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplittersNb(); ++iSplitter) {
        modelSettings.SelectHydroUnitSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);

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
        m_logger.SetDate(m_timer.GetDate());
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

bool ModelHydro::DumpOutputs(const wxString &path) {
    return m_logger.DumpOutputs(path);
}

bool ModelHydro::AddTimeSeries(TimeSeries* timeSeries) {
    for (auto ts: m_timeSeries) {
        if (ts->GetVariableType() == timeSeries->GetVariableType()) {
            wxLogError(_("The data variable is already linked to the model."));
            return false;
        }
    }

    if (timeSeries->GetStart() > m_timer.GetStart()) {
        wxLogError(_("The data starts after the beginning of the modelling period."));
        return false;
    }

    if (timeSeries->GetEnd() < m_timer.GetEnd()) {
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
