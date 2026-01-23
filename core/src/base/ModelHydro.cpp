#include "ModelHydro.h"

#include <memory>

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
        _processor.Initialize(modelSettings.GetSolverSettings());
        if (modelSettings.LogAll()) {
            _logger.RecordFractions();
        }
        _logger.InitContainers(_timer.GetTimeStepCount(), _subBasin, modelSettings);
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
    if (modelSettings.GetStructureCount() > 1) {
        throw NotImplemented(wxString::Format("ModelHydro::BuildModelStructure - Multiple structures (%d) not yet supported",
                                               modelSettings.GetStructureCount()));
    }

    modelSettings.SelectStructure(1);

    CreateSubBasinComponents(modelSettings);
    CreateHydroUnitsComponents(modelSettings);
}

void ModelHydro::UpdateParameters(SettingsModel& modelSettings) {
    if (modelSettings.GetStructureCount() > 1) {
        throw NotImplemented(wxString::Format("ModelHydro::UpdateParameters - Multiple structures (%d) not yet supported",
                                               modelSettings.GetStructureCount()));
    }

    modelSettings.SelectStructure(1);

    UpdateSubBasinParameters(modelSettings);
    UpdateHydroUnitsParameters(modelSettings);
}

void ModelHydro::CreateSubBasinComponents(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBrickCount(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        const BrickSettings& brickSettings = modelSettings.GetSubBasinBrickSettings(iBrick);

        // Create the brick
        auto brickPtr = std::unique_ptr<Brick>(Brick::Factory(brickSettings));
        Brick* brick = brickPtr.get();
        brick->SetName(brickSettings.name);
        brick->SetParameters(brickSettings);
        _subBasin->AddBrick(std::move(brickPtr));

        // Create the processes
        for (int iProcess = 0; iProcess < modelSettings.GetProcesseCount(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            auto processPtr = std::unique_ptr<Process>(Process::Factory(processSettings, brick));
            Process* process = processPtr.get();
            process->SetName(processSettings.name);
            process->SetParameters(processSettings);
            brick->AddProcess(std::move(processPtr));

            if (processSettings.type == "overflow") {
                brick->GetWaterContainer()->LinkOverflow(process);
            }
        }
    }

    // Create the splitters
    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplitterCount(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);

        auto splitterPtr = std::unique_ptr<Splitter>(Splitter::Factory(splitterSettings));
        Splitter* splitter = splitterPtr.get();
        splitter->SetName(splitterSettings.name);
        _subBasin->AddSplitter(std::move(splitterPtr));
    }

    LinkSubBasinProcessesTargetBricks(modelSettings);
    BuildSubBasinBricksFluxes(modelSettings);
    BuildSubBasinSplittersFluxes(modelSettings);
}

void ModelHydro::CreateHydroUnitsComponents(SettingsModel& modelSettings) {
    // Create the hydro unit bricks and splitters
    vecInt surfaceCompIndices = modelSettings.GetSurfaceComponentBricksIndices();
    vecInt landCoversIndices = modelSettings.GetLandCoverBricksIndices();
    int hydroUnitCount = _subBasin->GetHydroUnitCount();

    for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
        HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);

        // Create the surface component bricks
        for (int iBrick : surfaceCompIndices) {
            CreateHydroUnitBrick(modelSettings, unit, iBrick);
        }

        // Create the land cover bricks
        for (int iBrick : landCoversIndices) {
            CreateHydroUnitBrick(modelSettings, unit, iBrick);
        }

        // Create the other bricks
        for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBrickCount(); ++iBrick) {
            if (std::find(surfaceCompIndices.begin(), surfaceCompIndices.end(), iBrick) != surfaceCompIndices.end()) {
                continue;
            }
            if (std::find(landCoversIndices.begin(), landCoversIndices.end(), iBrick) != landCoversIndices.end()) {
                continue;
            }
            CreateHydroUnitBrick(modelSettings, unit, iBrick);
        }

        // Create the splitters
        for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplitterCount(); ++iSplitter) {
            modelSettings.SelectHydroUnitSplitter(iSplitter);
            const SplitterSettings& splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);

            auto splitterPtr = std::unique_ptr<Splitter>(Splitter::Factory(splitterSettings));
            Splitter* splitter = splitterPtr.get();
            splitter->SetName(splitterSettings.name);
            unit->AddSplitter(std::move(splitterPtr));

            BuildForcingConnections(splitterSettings, unit, splitter);
        }

        LinkSurfaceComponentsParents(modelSettings, unit);
    }

    // Create fluxes for the hydro units
    for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
        HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);
        LinkHydroUnitProcessesTargetBricks(modelSettings, unit);
        BuildHydroUnitBricksFluxes(modelSettings, unit);
        BuildHydroUnitSplittersFluxes(modelSettings, unit);
    }
}

void ModelHydro::CreateHydroUnitBrick(SettingsModel& modelSettings, HydroUnit* unit, int iBrick) {
    modelSettings.SelectHydroUnitBrick(iBrick);
    const BrickSettings& brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrick);

    auto brickPtr = std::unique_ptr<Brick>(Brick::Factory(brickSettings));
    Brick* brick = brickPtr.get();
    brick->SetName(brickSettings.name);
    brick->SetParameters(brickSettings);
    unit->AddBrick(std::move(brickPtr));

    BuildForcingConnections(brickSettings, unit, brick);

    // Create the processes
    for (int iProcess = 0; iProcess < modelSettings.GetProcesseCount(); ++iProcess) {
        modelSettings.SelectProcess(iProcess);
        const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

        auto processPtr = std::unique_ptr<Process>(Process::Factory(processSettings, brick));
        Process* process = processPtr.get();
        process->SetName(processSettings.name);
        process->SetHydroUnitProperties(unit, brick);
        process->SetParameters(processSettings);
        brick->AddProcess(std::move(processPtr));

        if (processSettings.type == "overflow") {
            brick->GetWaterContainer()->LinkOverflow(process);
        }

        BuildForcingConnections(processSettings, unit, process);
    }
}

void ModelHydro::UpdateSubBasinParameters(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBrickCount(); ++iBrick) {
        // Update the brick
        modelSettings.SelectSubBasinBrick(iBrick);
        const BrickSettings& brickSettings = modelSettings.GetSubBasinBrickSettings(iBrick);
        Brick* brick = _subBasin->GetBrick(iBrick);
        brick->SetParameters(brickSettings);

        // Update the processes
        for (int iProcess = 0; iProcess < modelSettings.GetProcesseCount(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);
            Process* process = brick->GetProcess(iProcess);
            process->SetParameters(processSettings);
        }
    }

    // Update the splitters
    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplitterCount(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);
        Splitter* splitter = _subBasin->GetSplitter(iSplitter);
        splitter->SetParameters(splitterSettings);
    }
}

void ModelHydro::UpdateHydroUnitsParameters(SettingsModel& modelSettings) {
    for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitCount(); ++iUnit) {
        HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);

        // Update the bricks for the hydro unit
        for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBrickCount(); ++iBrick) {
            modelSettings.SelectHydroUnitBrick(iBrick);
            const BrickSettings& brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrick);
            Brick* brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrick).name);
            brick->SetParameters(brickSettings);

            // Update the processes
            for (int iProcess = 0; iProcess < modelSettings.GetProcesseCount(); ++iProcess) {
                modelSettings.SelectProcess(iProcess);
                const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);
                Process* process = brick->GetProcess(iProcess);
                process->SetParameters(processSettings);
            }
        }

        // Update the splitters
        for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplitterCount(); ++iSplitter) {
            modelSettings.SelectHydroUnitSplitter(iSplitter);
            const SplitterSettings& splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);
            Splitter* splitter = unit->GetSplitter(iSplitter);
            splitter->SetParameters(splitterSettings);
        }
    }
}

void ModelHydro::LinkSurfaceComponentsParents(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetSurfaceComponentBrickCount(); ++iBrick) {
        const BrickSettings& brickSettings = modelSettings.GetSurfaceComponentBrickSettings(iBrick);
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
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBrickCount(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcesseCount(); ++iProcess) {
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = _subBasin->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            if (process->NeedsTargetBrickLinking()) {
                if (processSettings.outputs.size() != 1) {
                    throw ModelConfigError(_("There can only be a single process output for brick linking."));
                }
                Brick* targetBrick = _subBasin->GetBrick(processSettings.outputs[0].target);
                process->SetTargetBrick(targetBrick);
            }
        }
    }
}

void ModelHydro::LinkHydroUnitProcessesTargetBricks(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBrickCount(); ++iBrick) {
        modelSettings.SelectHydroUnitBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcesseCount(); ++iProcess) {
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrick).name);
            Process* process = brick->GetProcess(iProcess);

            if (process->NeedsTargetBrickLinking()) {
                if (processSettings.outputs.size() != 1) {
                    throw ModelConfigError(_("There can only be a single process output for brick linking."));
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
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBrickCount(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcesseCount(); ++iProcess) {
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = _subBasin->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            // Water goes to the atmosphere (ET)
            if (process->ToAtmosphere()) {
                auto fluxPtr = std::make_unique<FluxToAtmosphere>();
                process->AttachFluxOut(std::move(fluxPtr));
                continue;
            }

            for (const auto& output : processSettings.outputs) {
                std::unique_ptr<Flux> fluxPtr;
                Flux* flux;

                if (output.target == "outlet") {
                    // Water goes to the outlet
                    fluxPtr = std::make_unique<FluxToOutlet>();
                    flux = fluxPtr.get();
                    flux->SetType(output.fluxType);
                    _subBasin->AttachOutletFlux(flux);

                } else if (_subBasin->HasBrick(output.target)) {
                    // Water goes to another brick
                    Brick* targetBrick = _subBasin->GetBrick(output.target);
                    if (output.isInstantaneous) {
                        fluxPtr = std::make_unique<FluxToBrickInstantaneous>(targetBrick);
                        flux = fluxPtr.get();
                        flux->SetType(output.fluxType);
                        targetBrick->AttachFluxIn(flux);
                    } else {
                        fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                        flux = fluxPtr.get();
                        flux->SetType(output.fluxType);
                        targetBrick->AttachFluxIn(flux);
                    }

                } else if (_subBasin->HasSplitter(output.target)) {
                    // Water goes to a splitter
                    Splitter* targetSplitter = _subBasin->GetSplitter(output.target);
                    fluxPtr = std::make_unique<FluxSimple>();
                    flux = fluxPtr.get();
                    flux->SetType(output.fluxType);
                    flux->SetAsStatic();
                    targetSplitter->AttachFluxIn(flux);

                } else {
                    throw ModelConfigError(
                        wxString::Format(_("The target %s to attach the flux was no found"), output.target));
                }

                process->AttachFluxOut(std::move(fluxPtr));
            }
        }
    }
}

void ModelHydro::BuildHydroUnitBricksFluxes(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBrickCount(); ++iBrick) {
        modelSettings.SelectHydroUnitBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcesseCount(); ++iProcess) {
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrick).name);
            Process* process = brick->GetProcess(iProcess);

            // Water goes to the atmosphere (ET)
            if (process->ToAtmosphere()) {
                auto fluxPtr = std::make_unique<FluxToAtmosphere>();
                process->AttachFluxOut(std::move(fluxPtr));
                continue;
            }

            for (const auto& output : processSettings.outputs) {
                if (output.target == "outlet") {
                    // Water goes to the outlet
                    auto fluxPtr = std::make_unique<FluxToOutlet>();
                    Flux* flux = fluxPtr.get();
                    flux->SetAsStatic();
                    flux->SetType(output.fluxType);

                    // From hydro unit to basin: weight by hydro unit area
                    flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());

                    // Weight by surface area
                    if (brick->CanHaveAreaFraction()) {
                        flux->NeedsWeighting(true);
                    }
                    _subBasin->AttachOutletFlux(flux);
                    process->AttachFluxOut(std::move(fluxPtr));

                } else if (output.target == "lateral") {
                    // Cast the process to a lateral process
                    wxASSERT(process->IsLateralProcess());
                    auto lateralProcess = dynamic_cast<ProcessLateral*>(process);

                    // Get target unit from lateral connections
                    for (auto& lateralConnection : unit->GetLateralConnections()) {
                        HydroUnit* receiver = lateralConnection->GetReceiver();

                        if (output.fluxType == ContentType::Snow) {
                            // Look over the snowpack bricks of the receiver unit
                            for (auto& targetBrick : receiver->GetSnowpacks()) {
                                // Create the flux
                                std::unique_ptr<Flux> fluxPtr;
                                if (output.isInstantaneous) {
                                    fluxPtr = std::make_unique<FluxToBrickInstantaneous>(targetBrick);
                                } else {
                                    fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                                }

                                Flux* flux = fluxPtr.get();
                                if (output.isStatic) {
                                    flux->SetAsStatic();
                                }

                                flux->SetType(output.fluxType);

                                // Weight by surface area
                                flux->NeedsWeighting(true);

                                targetBrick->AttachFluxIn(flux);
                                lateralProcess->AttachFluxOutWithWeight(std::move(fluxPtr), lateralConnection->GetFraction());
                            }
                        } else {
                            throw NotImplemented(wxString::Format("ModelHydro::CreateHydroUnitsComponents - Lateral flux type %d not yet supported",
                                                                   static_cast<int>(output.fluxType)));
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
                    std::unique_ptr<Flux> fluxPtr;
                    if (output.isInstantaneous) {
                        fluxPtr = std::make_unique<FluxToBrickInstantaneous>(targetBrick);
                    } else {
                        fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                    }

                    Flux* flux = fluxPtr.get();
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
                    process->AttachFluxOut(std::move(fluxPtr));

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
                    auto fluxPtr = std::make_unique<FluxSimple>();
                    Flux* flux = fluxPtr.get();
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
                    process->AttachFluxOut(std::move(fluxPtr));

                } else {
                    throw ModelConfigError(
                        wxString::Format(_("The target %s to attach the flux was no found"), output.target));
                }
            }
        }
    }
}

void ModelHydro::BuildSubBasinSplittersFluxes(SettingsModel& modelSettings) {
    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplitterCount(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);

        Splitter* splitter = _subBasin->GetSplitter(iSplitter);

        for (const auto& output : splitterSettings.outputs) {
            std::unique_ptr<Flux> fluxPtr;
            Flux* flux;

            if (output.target == "outlet") {
                // Water goes to the outlet
                fluxPtr = std::make_unique<FluxToOutlet>();
                flux = fluxPtr.get();
                flux->SetType(output.fluxType);
                _subBasin->AttachOutletFlux(flux);

            } else if (_subBasin->HasBrick(output.target)) {
                // Look for target brick
                Brick* targetBrick = _subBasin->GetBrick(output.target);
                fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                flux = fluxPtr.get();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);
                targetBrick->AttachFluxIn(flux);

            } else if (_subBasin->HasSplitter(output.target)) {
                // Look for target splitter
                Splitter* targetSplitter = _subBasin->GetSplitter(output.target);
                fluxPtr = std::make_unique<FluxSimple>();
                flux = fluxPtr.get();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);
                targetSplitter->AttachFluxIn(flux);

            } else {
                throw ModelConfigError(
                    wxString::Format(_("The target %s to attach the flux was no found"), output.target));
            }

            splitter->AttachFluxOut(std::move(fluxPtr));
        }
    }
}

void ModelHydro::BuildHydroUnitSplittersFluxes(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplitterCount(); ++iSplitter) {
        modelSettings.SelectHydroUnitSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);

        Splitter* splitter = unit->GetSplitter(iSplitter);

        for (const auto& output : splitterSettings.outputs) {
            std::unique_ptr<Flux> fluxPtr;
            Flux* flux;

            if (output.target == "outlet") {
                // Water goes to the outlet
                fluxPtr = std::make_unique<FluxToOutlet>();
                flux = fluxPtr.get();
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
                fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                flux = fluxPtr.get();
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
                fluxPtr = std::make_unique<FluxSimple>();
                flux = fluxPtr.get();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);

                // From hydro unit to basin: weight by hydro unit area
                if (toSubBasin) {
                    flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());  // From hydro unit to basin
                }

                targetSplitter->AttachFluxIn(flux);

            } else {
                throw ModelConfigError(
                    wxString::Format(_("The target %s to attach the flux was no found"), output.target));
            }

            splitter->AttachFluxOut(std::move(fluxPtr));
        }
    }
}

void ModelHydro::BuildForcingConnections(const BrickSettings& brickSettings, HydroUnit* unit, Brick* brick) {
    for (auto forcingType : brickSettings.forcing) {
        if (!unit->HasForcing(forcingType)) {
            unit->AddForcing(std::make_unique<Forcing>(forcingType));
        }

        auto forcing = unit->GetForcing(forcingType);
        auto forcingFlux = new FluxForcing();
        forcingFlux->AttachForcing(forcing);
        brick->AttachFluxIn(forcingFlux);
    }
}

void ModelHydro::BuildForcingConnections(const ProcessSettings& processSettings, HydroUnit* unit, Process* process) {
    for (auto forcingType : processSettings.forcing) {
        if (!unit->HasForcing(forcingType)) {
            unit->AddForcing(std::make_unique<Forcing>(forcingType));
        }

        auto forcing = unit->GetForcing(forcingType);
        process->AttachForcing(forcing);
    }
}

void ModelHydro::BuildForcingConnections(const SplitterSettings& splitterSettings, HydroUnit* unit, Splitter* splitter) {
    for (auto forcingType : splitterSettings.forcing) {
        if (!unit->HasForcing(forcingType)) {
            unit->AddForcing(std::make_unique<Forcing>(forcingType));
        }

        auto forcing = unit->GetForcing(forcingType);
        splitter->AttachForcing(forcing);
    }
}

void ModelHydro::ConnectLoggerToValues(SettingsModel& modelSettings) {
    if (modelSettings.GetStructureCount() > 1) {
        throw NotImplemented(wxString::Format("ModelHydro::ConnectLoggerToValues - Multiple structures (%d) not yet supported",
                                               modelSettings.GetStructureCount()));
    }

    double* valPt = nullptr;

    // Sub basin values
    int iLabel = 0;

    for (int iBrickType = 0; iBrickType < modelSettings.GetSubBasinBrickCount(); ++iBrickType) {
        modelSettings.SelectSubBasinBrick(iBrickType);
        const BrickSettings& brickSettings = modelSettings.GetSubBasinBrickSettings(iBrickType);

        for (const auto& logItem : brickSettings.logItems) {
            valPt = _subBasin->GetBrick(iBrickType)->GetBaseValuePointer(logItem);
            if (valPt == nullptr) {
                valPt = _subBasin->GetBrick(iBrickType)->GetValuePointer(logItem);
            }
            if (valPt == nullptr) {
                throw ShouldNotHappen(wxString::Format("ModelHydro::ConnectLoggerToValues - Log item '%s' not found in sub-basin brick %d",
                                                        logItem, iBrickType));
            }
            _logger.SetSubBasinValuePointer(iLabel, valPt);
            iLabel++;
        }

        for (int iProcess = 0; iProcess < modelSettings.GetProcesseCount(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            for (const auto& logItem : processSettings.logItems) {
                valPt = _subBasin->GetBrick(iBrickType)->GetProcess(iProcess)->GetValuePointer(logItem);
                if (valPt == nullptr) {
                    throw ShouldNotHappen(wxString::Format("ModelHydro::ConnectLoggerToValues - Log item '%s' not found in sub-basin process %d of brick %d",
                                                            logItem, iProcess, iBrickType));
                }
                _logger.SetSubBasinValuePointer(iLabel, valPt);
                iLabel++;
            }
        }
    }

    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplitterCount(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);

        for (const auto& logItem : splitterSettings.logItems) {
            valPt = _subBasin->GetSplitter(iSplitter)->GetValuePointer(logItem);
            if (valPt == nullptr) {
                throw ShouldNotHappen(wxString::Format("ModelHydro::ConnectLoggerToValues - Log item '%s' not found in sub-basin splitter %d",
                                                        logItem, iSplitter));
            }
            _logger.SetSubBasinValuePointer(iLabel, valPt);
            iLabel++;
        }
    }

    vecStr genericLogLabels = modelSettings.GetSubBasinGenericLogLabels();
    for (auto& genericLogLabel : genericLogLabels) {
        valPt = _subBasin->GetValuePointer(genericLogLabel);
        if (valPt == nullptr) {
            throw ShouldNotHappen(wxString::Format("ModelHydro::ConnectLoggerToValues - Generic log label '%s' not found in sub-basin",
                                                    genericLogLabel));
        }
        _logger.SetSubBasinValuePointer(iLabel, valPt);
        iLabel++;
    }

    // Hydro units values
    iLabel = 0;

    for (int iBrickType = 0; iBrickType < modelSettings.GetHydroUnitBrickCount(); ++iBrickType) {
        modelSettings.SelectHydroUnitBrick(iBrickType);
        const BrickSettings& brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrickType);

        for (const auto& logItem : brickSettings.logItems) {
            for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitCount(); ++iUnit) {
                auto unit = _subBasin->GetHydroUnit(iUnit);
                auto brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrickType).name);
                valPt = brick->GetBaseValuePointer(logItem);
                if (valPt == nullptr) {
                    valPt = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrickType).name)
                                ->GetValuePointer(logItem);
                }
                if (valPt == nullptr) {
                    throw ShouldNotHappen(wxString::Format("ModelHydro::ConnectLoggerToValues - Log item '%s' not found in hydro unit %d brick %d",
                                                            logItem, iUnit, iBrickType));
                }
                _logger.SetHydroUnitValuePointer(iUnit, iLabel, valPt);
            }
            iLabel++;
        }

        for (int iProcess = 0; iProcess < modelSettings.GetProcesseCount(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            for (const auto& logItem : processSettings.logItems) {
                for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitCount(); ++iUnit) {
                    auto unit = _subBasin->GetHydroUnit(iUnit);
                    auto brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrickType).name);
                    auto process = brick->GetProcess(iProcess);
                    valPt = process->GetValuePointer(logItem);
                    if (valPt == nullptr) {
                        throw ShouldNotHappen(wxString::Format("ModelHydro::ConnectLoggerToValues - Log item '%s' not found in hydro unit %d process %d of brick %d",
                                                                logItem, iUnit, iProcess, iBrickType));
                    }
                    _logger.SetHydroUnitValuePointer(iUnit, iLabel, valPt);
                }
                iLabel++;
            }
        }
    }

    // Splitter values

    for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplitterCount(); ++iSplitter) {
        modelSettings.SelectHydroUnitSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);

        for (const auto& logItem : splitterSettings.logItems) {
            for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitCount(); ++iUnit) {
                HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);
                valPt = unit->GetSplitter(iSplitter)->GetValuePointer(logItem);
                if (valPt == nullptr) {
                    throw ShouldNotHappen(wxString::Format("ModelHydro::ConnectLoggerToValues - Log item '%s' not found in hydro unit %d splitter %d",
                                                            logItem, iUnit, iSplitter));
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
        const BrickSettings& brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrickType);

        for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitCount(); ++iUnit) {
            HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);
            LandCover* brick = dynamic_cast<LandCover*>(unit->GetBrick(brickSettings.name));
            valPt = brick->GetAreaFractionPointer();

            if (valPt == nullptr) {
                throw ShouldNotHappen(wxString::Format("ModelHydro::ConnectLoggerToValues - Area fraction pointer not found for land cover brick '%s' in unit %d",
                                                        brickSettings.name, iUnit));
            }
            _logger.SetHydroUnitFractionPointer(iUnit, iLabel, valPt);
        }
        iLabel++;
    }
}

bool ModelHydro::IsValid() const {
    if (!_subBasin->IsValid()) return false;

    return true;
}

void ModelHydro::Validate() const {
    _subBasin->Validate();
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
        if (!_processor.ProcessTimeStep(*_timer.GetTimeStepPointer())) {
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
    for (auto& ts : _timeSeries) {
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
        auto timeSeriesPtr = std::unique_ptr<TimeSeries>(TimeSeries::Create(varName, time, ids, data));
        if (!AddTimeSeries(std::move(timeSeriesPtr))) {
            return false;
        }
    } catch (const std::exception& e) {
        wxLogError(_("An exception occurred during timeseries creation: %s."), e.what());
        return false;
    }

    return true;
}

void ModelHydro::ClearTimeSeries() {
    _timeSeries.clear();  // Automatic cleanup via unique_ptr
}

bool ModelHydro::AttachTimeSeriesToHydroUnits() {
    wxASSERT(_subBasin);

    for (const auto& timeSeries : _timeSeries) {
        VariableType type = timeSeries->GetVariableType();

        for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitCount(); ++iUnit) {
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
    for (const auto& timeSeries : _timeSeries) {
        wxASSERT(timeSeries);
        if (!timeSeries->SetCursorToDate(_timer.GetDate())) {
            return false;
        }
    }

    return true;
}

bool ModelHydro::UpdateForcing() {
    for (const auto& timeSeries : _timeSeries) {
        if (!timeSeries->AdvanceOneTimeStep()) {
            return false;
        }
    }

    return true;
}
