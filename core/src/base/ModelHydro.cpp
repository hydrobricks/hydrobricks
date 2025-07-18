#include "ModelHydro.h"

#include "FluxForcing.h"
#include "FluxSimple.h"
#include "FluxToAtmosphere.h"
#include "FluxToBrick.h"
#include "FluxToBrickInstantaneous.h"
#include "FluxToOutlet.h"
#include "Includes.h"
#include "LandCover.h"
#include "ProcessLateral.h"
#include "SurfaceComponent.h"

ModelHydro::ModelHydro(SubBasin* subBasin)
    : _subBasin(subBasin) {
    _processor.SetModel(this);
    _actionsManager.SetModel(this);
    _timer.SetActionsManager(&_actionsManager);
    _timer.SetParametersUpdater(&_parametersUpdater);
}

ModelHydro::~ModelHydro() = default;

bool ModelHydro::InitializeWithBasin(SettingsModel& modelSettings, SettingsBasin& basinSettings) {
    wxDELETE(_subBasin);
    _subBasin = new SubBasin();
    if (!_subBasin->Initialize(basinSettings)) {
        return false;
    }
    if (!Initialize(modelSettings, basinSettings)) {
        return false;
    }

    return true;
}

bool ModelHydro::Initialize(SettingsModel& modelSettings, SettingsBasin& basinSettings) {
    try {
        BuildModelStructure(modelSettings);

        _timer.Initialize(modelSettings.GetTimerSettings());
        config::timeStepInDays = *_timer.GetTimeStepPointer();
        _processor.Initialize(modelSettings.GetSolverSettings());
        if (modelSettings.LogAll()) {
            _logger.RecordFractions();
        }
        _logger.InitContainers(_timer.GetTimeStepsNb(), _subBasin, modelSettings);
        if (!_subBasin->AssignFractions(basinSettings)) {
            return false;
        }
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

void ModelHydro::UpdateParameters(SettingsModel& modelSettings) {
    if (modelSettings.GetStructuresNb() > 1) {
        throw NotImplemented();
    }

    modelSettings.SelectStructure(1);

    UpdateSubBasinParameters(modelSettings);
    UpdateHydroUnitsParameters(modelSettings);
}

void ModelHydro::CreateSubBasinComponents(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBricksNb(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        BrickSettings brickSettings = modelSettings.GetSubBasinBrickSettings(iBrick);

        // Create the brick
        Brick* brick = Brick::Factory(brickSettings);
        brick->SetName(brickSettings.name);
        brick->SetParameters(brickSettings);
        _subBasin->AddBrick(brick);

        // Create the processes
        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            Process* process = Process::Factory(processSettings, brick);
            process->SetName(processSettings.name);
            process->SetParameters(processSettings);
            brick->AddProcess(process);

            if (processSettings.type == "overflow") {
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
        _subBasin->AddSplitter(splitter);
    }

    LinkSubBasinProcessesTargetBricks(modelSettings);
    BuildSubBasinBricksFluxes(modelSettings);
    BuildSubBasinSplittersFluxes(modelSettings);
}

void ModelHydro::CreateHydroUnitsComponents(SettingsModel& modelSettings) {
    // Create the hydro unit bricks and splitters
    for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitsNb(); ++iUnit) {
        HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);

        vecInt surfaceCompIndices = modelSettings.GetSurfaceComponentBricksIndices();
        vecInt landCoversIndices = modelSettings.GetLandCoverBricksIndices();

        // Create the surface component bricks
        for (int iBrick : surfaceCompIndices) {
            CreateHydroUnitBrick(modelSettings, unit, iBrick);
        }

        // Create the land cover bricks
        for (int iBrick : landCoversIndices) {
            CreateHydroUnitBrick(modelSettings, unit, iBrick);
        }

        // Create the other bricks
        for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBricksNb(); ++iBrick) {
            if (std::find(surfaceCompIndices.begin(), surfaceCompIndices.end(), iBrick) != surfaceCompIndices.end()) {
                continue;
            }
            if (std::find(landCoversIndices.begin(), landCoversIndices.end(), iBrick) != landCoversIndices.end()) {
                continue;
            }
            CreateHydroUnitBrick(modelSettings, unit, iBrick);
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

        LinkSurfaceComponentsParents(modelSettings, unit);
    }

    // Create fluxes for the hydro units
    for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitsNb(); ++iUnit) {
        HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);
        LinkHydroUnitProcessesTargetBricks(modelSettings, unit);
        BuildHydroUnitBricksFluxes(modelSettings, unit);
        BuildHydroUnitSplittersFluxes(modelSettings, unit);
    }
}

void ModelHydro::CreateHydroUnitBrick(SettingsModel& modelSettings, HydroUnit* unit, int iBrick) {
    modelSettings.SelectHydroUnitBrick(iBrick);
    BrickSettings brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrick);

    Brick* brick = Brick::Factory(brickSettings);
    brick->SetName(brickSettings.name);
    brick->SetParameters(brickSettings);
    unit->AddBrick(brick);

    BuildForcingConnections(brickSettings, unit, brick);

    // Create the processes
    for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
        modelSettings.SelectProcess(iProcess);
        ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

        Process* process = Process::Factory(processSettings, brick);
        process->SetName(processSettings.name);
        process->SetHydroUnitProperties(unit, brick);
        process->SetParameters(processSettings);
        brick->AddProcess(process);

        if (processSettings.type == "overflow") {
            brick->GetWaterContainer()->LinkOverflow(process);
        }

        BuildForcingConnections(processSettings, unit, process);
    }
}

void ModelHydro::UpdateSubBasinParameters(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBricksNb(); ++iBrick) {
        // Update the brick
        modelSettings.SelectSubBasinBrick(iBrick);
        BrickSettings brickSettings = modelSettings.GetSubBasinBrickSettings(iBrick);
        Brick* brick = _subBasin->GetBrick(iBrick);
        brick->SetParameters(brickSettings);

        // Update the processes
        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);
            Process* process = brick->GetProcess(iProcess);
            process->SetParameters(processSettings);
        }
    }

    // Update the splitters
    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplittersNb(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);
        Splitter* splitter = _subBasin->GetSplitter(iSplitter);
        splitter->SetParameters(splitterSettings);
    }
}

void ModelHydro::UpdateHydroUnitsParameters(SettingsModel& modelSettings) {
    for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitsNb(); ++iUnit) {
        HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);

        // Update the bricks for the hydro unit
        for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBricksNb(); ++iBrick) {
            modelSettings.SelectHydroUnitBrick(iBrick);
            BrickSettings brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrick);
            Brick* brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrick).name);
            brick->SetParameters(brickSettings);

            // Update the processes
            for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
                modelSettings.SelectProcess(iProcess);
                ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);
                Process* process = brick->GetProcess(iProcess);
                process->SetParameters(processSettings);
            }
        }

        // Update the splitters
        for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplittersNb(); ++iSplitter) {
            modelSettings.SelectHydroUnitSplitter(iSplitter);
            SplitterSettings splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);
            Splitter* splitter = unit->GetSplitter(iSplitter);
            splitter->SetParameters(splitterSettings);
        }
    }
}

void ModelHydro::LinkSurfaceComponentsParents(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetSurfaceComponentBricksNb(); ++iBrick) {
        BrickSettings brickSettings = modelSettings.GetSurfaceComponentBrickSettings(iBrick);
        if (!brickSettings.parent.empty()) {
            auto surfaceComponentBrick = dynamic_cast<SurfaceComponent*>(unit->GetBrick(brickSettings.name));
            auto landCoverBrick = dynamic_cast<LandCover*>(unit->GetBrick(brickSettings.parent));
            wxASSERT(surfaceComponentBrick);
            wxASSERT(landCoverBrick);
            surfaceComponentBrick->SetParent(landCoverBrick);
        }
    }
}

void ModelHydro::LinkSubBasinProcessesTargetBricks(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBricksNb(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = _subBasin->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            if (process->NeedsTargetBrickLinking()) {
                if (processSettings.outputs.size() != 1) {
                    throw ConceptionIssue(_("There can only be a single process output for brick linking."));
                }
                Brick* targetBrick = _subBasin->GetBrick(processSettings.outputs[0].target);
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

            Brick* brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrick).name);
            Process* process = brick->GetProcess(iProcess);

            if (process->NeedsTargetBrickLinking()) {
                if (processSettings.outputs.size() != 1) {
                    throw ConceptionIssue(_("There can only be a single process output for brick linking."));
                }
                Brick* targetBrick = nullptr;
                if (unit->HasBrick(processSettings.outputs[0].target)) {
                    targetBrick = unit->GetBrick(processSettings.outputs[0].target);
                } else {
                    targetBrick = _subBasin->GetBrick(processSettings.outputs[0].target);
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
            Brick* brick = _subBasin->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            // Water goes to the atmosphere (ET)
            if (process->ToAtmosphere()) {
                flux = new FluxToAtmosphere();
                process->AttachFluxOut(flux);
                continue;
            }

            for (const auto& output : processSettings.outputs) {
                if (output.target == "outlet") {
                    // Water goes to the outlet
                    flux = new FluxToOutlet();
                    flux->SetType(output.fluxType);
                    _subBasin->AttachOutletFlux(flux);

                } else if (_subBasin->HasBrick(output.target)) {
                    // Water goes to another brick
                    Brick* targetBrick = _subBasin->GetBrick(output.target);
                    if (output.isInstantaneous) {
                        flux = new FluxToBrickInstantaneous(targetBrick);
                        flux->SetType(output.fluxType);
                        targetBrick->AttachFluxIn(flux);
                    } else {
                        flux = new FluxToBrick(targetBrick);
                        flux->SetType(output.fluxType);
                        targetBrick->AttachFluxIn(flux);
                    }

                } else if (_subBasin->HasSplitter(output.target)) {
                    // Water goes to a splitter
                    Splitter* targetSplitter = _subBasin->GetSplitter(output.target);
                    flux = new FluxSimple();
                    flux->SetType(output.fluxType);
                    flux->SetAsStatic();
                    targetSplitter->AttachFluxIn(flux);

                } else {
                    throw ConceptionIssue(
                        wxString::Format(_("The target %s to attach the flux was no found"), output.target));
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

            Flux* flux = nullptr;
            Brick* brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrick).name);
            Process* process = brick->GetProcess(iProcess);

            // Water goes to the atmosphere (ET)
            if (process->ToAtmosphere()) {
                flux = new FluxToAtmosphere();
                process->AttachFluxOut(flux);
                continue;
            }

            for (const auto& output : processSettings.outputs) {
                if (output.target == "outlet") {
                    // Water goes to the outlet
                    flux = new FluxToOutlet();
                    flux->SetAsStatic();
                    flux->SetType(output.fluxType);

                    // From hydro unit to basin: weight by hydro unit area
                    flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());

                    // Weight by surface area
                    if (brick->CanHaveAreaFraction()) {
                        flux->NeedsWeighting(true);
                    }
                    _subBasin->AttachOutletFlux(flux);
                    process->AttachFluxOut(flux);

                } else if (output.target == "lateral") {
                    // Cast the process to a lateral process
                    wxASSERT(process->IsLateralProcess());
                    auto lateralProcess = dynamic_cast<ProcessLateral*>(process);

                    // Get target unit from lateral connections
                    for (auto& lateralConnection : unit->GetLateralConnections()) {
                        HydroUnit* receiver = lateralConnection->GetReceiver();

                        if (output.fluxType == "snow") {
                            // Look over the snowpack bricks of the receiver unit
                            for (auto& targetBrick : receiver->GetSnowpacks()) {
                                // Create the flux
                                if (output.isInstantaneous) {
                                    flux = new FluxToBrickInstantaneous(targetBrick);
                                } else {
                                    flux = new FluxToBrick(targetBrick);
                                }

                                if (output.isStatic) {
                                    flux->SetAsStatic();
                                }

                                flux->SetType(output.fluxType);

                                // Weight by surface area
                                flux->NeedsWeighting(true);

                                targetBrick->AttachFluxIn(flux);
                                lateralProcess->AttachFluxOutWithWeight(flux, lateralConnection->GetFraction());
                            }
                        } else {
                            throw NotImplemented();
                        }
                    }
                } else if (unit->HasBrick(output.target) || _subBasin->HasBrick(output.target)) {
                    bool toSubBasin = false;
                    Brick* targetBrick = nullptr;

                    // Look for target brick
                    if (unit->HasBrick(output.target)) {
                        targetBrick = unit->GetBrick(output.target);
                    } else {
                        targetBrick = _subBasin->GetBrick(output.target);
                        toSubBasin = true;
                    }

                    // Create the flux
                    if (output.isInstantaneous) {
                        flux = new FluxToBrickInstantaneous(targetBrick);
                    } else {
                        flux = new FluxToBrick(targetBrick);
                    }

                    flux->SetType(output.fluxType);

                    // If leaves surface components: weight by surface area
                    if (brick->CanHaveAreaFraction() && !targetBrick->CanHaveAreaFraction()) {
                        flux->NeedsWeighting(true);
                    }

                    // From hydro unit to basin: weight by hydro unit area
                    if (toSubBasin) {
                        flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());
                        flux->SetAsStatic();
                    } else if (output.isStatic) {
                        flux->SetAsStatic();
                    }

                    targetBrick->AttachFluxIn(flux);
                    process->AttachFluxOut(flux);

                } else if (unit->HasSplitter(output.target) || _subBasin->HasSplitter(output.target)) {
                    bool toSubBasin = false;
                    Splitter* targetSplitter = nullptr;

                    // Look for target splitter
                    if (unit->HasSplitter(output.target)) {
                        targetSplitter = unit->GetSplitter(output.target);
                    } else {
                        targetSplitter = _subBasin->GetSplitter(output.target);
                        toSubBasin = true;
                    }

                    // Create the flux
                    flux = new FluxSimple();
                    flux->SetType(output.fluxType);
                    flux->SetAsStatic();

                    // If coming from a surface component: weight by surface area
                    if (brick->CanHaveAreaFraction()) {
                        flux->NeedsWeighting(true);
                    }

                    // From hydro unit to basin: weight by hydro unit area
                    if (toSubBasin) {
                        flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());
                    }

                    targetSplitter->AttachFluxIn(flux);
                    process->AttachFluxOut(flux);

                } else {
                    throw ConceptionIssue(
                        wxString::Format(_("The target %s to attach the flux was no found"), output.target));
                }
            }
        }
    }
}

void ModelHydro::BuildSubBasinSplittersFluxes(SettingsModel& modelSettings) {
    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplittersNb(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);

        Splitter* splitter = _subBasin->GetSplitter(iSplitter);

        for (const auto& output : splitterSettings.outputs) {
            Flux* flux;
            if (output.target == "outlet") {
                // Water goes to the outlet
                flux = new FluxToOutlet();
                flux->SetType(output.fluxType);
                _subBasin->AttachOutletFlux(flux);

            } else if (_subBasin->HasBrick(output.target)) {
                // Look for target brick
                Brick* targetBrick = _subBasin->GetBrick(output.target);
                flux = new FluxToBrick(targetBrick);
                flux->SetAsStatic();
                flux->SetType(output.fluxType);
                targetBrick->AttachFluxIn(flux);

            } else if (_subBasin->HasSplitter(output.target)) {
                // Look for target splitter
                Splitter* targetSplitter = _subBasin->GetSplitter(output.target);
                flux = new FluxSimple();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);
                targetSplitter->AttachFluxIn(flux);

            } else {
                throw ConceptionIssue(
                    wxString::Format(_("The target %s to attach the flux was no found"), output.target));
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

        for (const auto& output : splitterSettings.outputs) {
            Flux* flux;
            if (output.target == "outlet") {
                // Water goes to the outlet
                flux = new FluxToOutlet();
                flux->SetType(output.fluxType);

                // From hydro unit to basin: weight by hydro unit area
                flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());

                _subBasin->AttachOutletFlux(flux);

            } else if (unit->HasBrick(output.target) || _subBasin->HasBrick(output.target)) {
                bool toSubBasin = false;
                Brick* targetBrick = nullptr;

                // Look for target brick
                if (unit->HasBrick(output.target)) {
                    targetBrick = unit->GetBrick(output.target);
                } else {
                    targetBrick = _subBasin->GetBrick(output.target);
                    toSubBasin = true;
                }

                // Create flux
                flux = new FluxToBrick(targetBrick);
                flux->SetAsStatic();
                flux->SetType(output.fluxType);

                // From hydro unit to basin: weight by hydro unit area
                if (toSubBasin) {
                    flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());  // From hydro unit to basin
                }

                targetBrick->AttachFluxIn(flux);

            } else if (unit->HasSplitter(output.target) || _subBasin->HasSplitter(output.target)) {
                bool toSubBasin = false;
                Splitter* targetSplitter = nullptr;

                // Look for the target splitter
                if (unit->HasSplitter(output.target)) {
                    targetSplitter = unit->GetSplitter(output.target);
                } else {
                    targetSplitter = _subBasin->GetSplitter(output.target);
                    toSubBasin = true;
                }

                // Create flux
                flux = new FluxSimple();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);

                // From hydro unit to basin: weight by hydro unit area
                if (toSubBasin) {
                    flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());  // From hydro unit to basin
                }

                targetSplitter->AttachFluxIn(flux);

            } else {
                throw ConceptionIssue(
                    wxString::Format(_("The target %s to attach the flux was no found"), output.target));
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

void ModelHydro::BuildForcingConnections(ProcessSettings& processSettings, HydroUnit* unit, Process* process) {
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
            valPt = _subBasin->GetBrick(iBrickType)->GetBaseValuePointer(logItem);
            if (valPt == nullptr) {
                valPt = _subBasin->GetBrick(iBrickType)->GetValuePointer(logItem);
            }
            if (valPt == nullptr) {
                throw ShouldNotHappen();
            }
            _logger.SetSubBasinValuePointer(iLabel, valPt);
            iLabel++;
        }

        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            for (const auto& logItem : processSettings.logItems) {
                valPt = _subBasin->GetBrick(iBrickType)->GetProcess(iProcess)->GetValuePointer(logItem);
                if (valPt == nullptr) {
                    throw ShouldNotHappen();
                }
                _logger.SetSubBasinValuePointer(iLabel, valPt);
                iLabel++;
            }
        }
    }

    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplittersNb(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);

        for (const auto& logItem : splitterSettings.logItems) {
            valPt = _subBasin->GetSplitter(iSplitter)->GetValuePointer(logItem);
            if (valPt == nullptr) {
                throw ShouldNotHappen();
            }
            _logger.SetSubBasinValuePointer(iLabel, valPt);
            iLabel++;
        }
    }

    vecStr genericLogLabels = modelSettings.GetSubBasinGenericLogLabels();
    for (auto& genericLogLabel : genericLogLabels) {
        valPt = _subBasin->GetValuePointer(genericLogLabel);
        if (valPt == nullptr) {
            throw ShouldNotHappen();
        }
        _logger.SetSubBasinValuePointer(iLabel, valPt);
        iLabel++;
    }

    // Hydro units values
    iLabel = 0;

    for (int iBrickType = 0; iBrickType < modelSettings.GetHydroUnitBricksNb(); ++iBrickType) {
        modelSettings.SelectHydroUnitBrick(iBrickType);
        BrickSettings brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrickType);

        for (const auto& logItem : brickSettings.logItems) {
            for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitsNb(); ++iUnit) {
                auto unit = _subBasin->GetHydroUnit(iUnit);
                auto brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrickType).name);
                valPt = brick->GetBaseValuePointer(logItem);
                if (valPt == nullptr) {
                    valPt = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrickType).name)
                                ->GetValuePointer(logItem);
                }
                if (valPt == nullptr) {
                    throw ShouldNotHappen();
                }
                _logger.SetHydroUnitValuePointer(iUnit, iLabel, valPt);
            }
            iLabel++;
        }

        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            for (const auto& logItem : processSettings.logItems) {
                for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitsNb(); ++iUnit) {
                    auto unit = _subBasin->GetHydroUnit(iUnit);
                    auto brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrickType).name);
                    auto process = brick->GetProcess(iProcess);
                    valPt = process->GetValuePointer(logItem);
                    if (valPt == nullptr) {
                        throw ShouldNotHappen();
                    }
                    _logger.SetHydroUnitValuePointer(iUnit, iLabel, valPt);
                }
                iLabel++;
            }
        }
    }

    // Splitter values

    for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplittersNb(); ++iSplitter) {
        modelSettings.SelectHydroUnitSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);

        for (const auto& logItem : splitterSettings.logItems) {
            for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitsNb(); ++iUnit) {
                HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);
                valPt = unit->GetSplitter(iSplitter)->GetValuePointer(logItem);
                if (valPt == nullptr) {
                    throw ShouldNotHappen();
                }
                _logger.SetHydroUnitValuePointer(iUnit, iLabel, valPt);
                iLabel++;
            }
        }
    }

    // Fractions
    iLabel = 0;

    for (int iBrickType : modelSettings.GetLandCoverBricksIndices()) {
        modelSettings.SelectHydroUnitBrick(iBrickType);
        BrickSettings brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrickType);

        for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitsNb(); ++iUnit) {
            HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);
            LandCover* brick = dynamic_cast<LandCover*>(unit->GetBrick(brickSettings.name));
            valPt = brick->GetAreaFractionPointer();

            if (valPt == nullptr) {
                throw ShouldNotHappen();
            }
            _logger.SetHydroUnitFractionPointer(iUnit, iLabel, valPt);
        }
        iLabel++;
    }
}

bool ModelHydro::IsOk() {
    if (!_subBasin->IsOk()) return false;

    return true;
}

bool ModelHydro::ForcingLoaded() {
    return !_timeSeries.empty();
}

bool ModelHydro::Run() {
    if (!InitializeTimeSeries()) {
        return false;
    }

    _logger.SaveInitialValues();

    wxLogMessage(_("Simulation starting."));

    while (!_timer.IsOver()) {
        if (!_processor.ProcessTimeStep()) {
            wxLogError(_("Failed running the model."));
            return false;
        }
        _logger.SetDate(_timer.GetDate());
        _logger.Record();
        _timer.IncrementTime();
        _logger.Increment();
        if (!UpdateForcing()) {
            wxLogError(_("Failed updating the forcing data."));
            return false;
        }
    }

    wxLogMessage(_("Simulation completed."));

    return true;
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

axd ModelHydro::GetOutletDischarge() {
    return _logger.GetOutletDischarge();
}

double ModelHydro::GetTotalOutletDischarge() {
    return _logger.GetTotalOutletDischarge();
}

double ModelHydro::GetTotalET() {
    return _logger.GetTotalET();
}

double ModelHydro::GetTotalWaterStorageChanges() {
    return _logger.GetTotalWaterStorageChanges();
}

double ModelHydro::GetTotalSnowStorageChanges() {
    return _logger.GetTotalSnowStorageChanges();
}

double ModelHydro::GetTotalGlacierStorageChanges() {
    return _logger.GetTotalGlacierStorageChanges();
}

bool ModelHydro::AddTimeSeries(TimeSeries* timeSeries) {
    for (auto ts : _timeSeries) {
        if (ts->GetVariableType() == timeSeries->GetVariableType()) {
            wxLogError(_("The data variable is already linked to the model."));
            return false;
        }
    }

    if (timeSeries->GetStart() > _timer.GetStart()) {
        wxLogError(_("The data starts after the beginning of the modelling period."));
        return false;
    }

    if (timeSeries->GetEnd() < _timer.GetEnd()) {
        wxLogError(_("The data ends before the end of the modelling period."));
        return false;
    }

    _timeSeries.push_back(timeSeries);

    return true;
}

bool ModelHydro::AddAction(Action* action) {
    return _actionsManager.AddAction(action);
}

int ModelHydro::GetActionsNb() {
    return _actionsManager.GetActionsNb();
}

int ModelHydro::GetSporadicActionItemsNb() {
    return _actionsManager.GetSporadicActionItemsNb();
}

bool ModelHydro::CreateTimeSeries(const string& varName, const axd& time, const axi& ids, const axxd& data) {
    try {
        TimeSeries* timeSeries = TimeSeries::Create(varName, time, ids, data);
        if (!AddTimeSeries(timeSeries)) {
            return false;
        }
    } catch (const std::exception& e) {
        wxLogError(_("An exception occurred during timeseries creation: %s."), e.what());
        return false;
    }

    return true;
}

void ModelHydro::ClearTimeSeries() {
    for (auto ts : _timeSeries) {
        wxDELETE(ts);
    }
    _timeSeries.resize(0);
}

bool ModelHydro::AttachTimeSeriesToHydroUnits() {
    wxASSERT(_subBasin);

    for (auto timeSeries : _timeSeries) {
        VariableType type = timeSeries->GetVariableType();

        for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitsNb(); ++iUnit) {
            HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);
            if (unit->HasForcing(type)) {
                Forcing* forcing = unit->GetForcing(type);
                forcing->AttachTimeSeriesData(timeSeries->GetDataPointer(unit->GetId()));
            }
        }
    }

    return true;
}

bool ModelHydro::InitializeTimeSeries() {
    for (auto timeSeries : _timeSeries) {
        wxASSERT(timeSeries);
        if (!timeSeries->SetCursorToDate(_timer.GetDate())) {
            return false;
        }
    }

    return true;
}

bool ModelHydro::UpdateForcing() {
    for (auto timeSeries : _timeSeries) {
        if (!timeSeries->AdvanceOneTimeStep()) {
            return false;
        }
    }

    return true;
}
