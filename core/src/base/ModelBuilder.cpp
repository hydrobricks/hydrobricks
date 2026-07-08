#include "ModelBuilder.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>

#include "FluxForcing.h"
#include "FluxSimple.h"
#include "FluxToAtmosphere.h"
#include "FluxToBrick.h"
#include "FluxToBrickInstantaneous.h"
#include "FluxToOutlet.h"
#include "Forcing.h"
#include "HydroUnit.h"
#include "LandCover.h"
#include "Logger.h"
#include "Process.h"
#include "ProcessLateral.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "Splitter.h"
#include "SubBasin.h"
#include "SurfaceComponent.h"
#include "TimeMachine.h"

ModelBuilder::ModelBuilder(SubBasin* subBasin, TimeMachine* timer, Logger* logger)
    : _subBasin(subBasin),
      _timer(timer),
      _logger(logger) {
    assert(subBasin);
    assert(timer);
    assert(logger);
}

void ModelBuilder::AssignHydroUnitStructures(SettingsModel& modelSettings, SettingsBasin& basinSettings) {
    int structureCount = modelSettings.GetStructureCount();
    if (structureCount <= 1) {
        return;  // single structure: every unit keeps the default id 1
    }

    // Land-cover name set of each structure variant (ids are 1..structureCount).
    vector<std::set<string>> structureCovers(structureCount + 1);
    for (int id = 1; id <= structureCount; ++id) {
        modelSettings.SelectStructure(id);
        vecStr covers = modelSettings.GetSelectedStructureLandCoverNames();
        structureCovers[id] = std::set<string>(covers.begin(), covers.end());
    }

    for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitCount(); ++iUnit) {
        basinSettings.SelectUnit(iUnit);

        // Land covers actually present in this unit (non-zero fraction).
        std::set<string> present;
        for (int iLC = 0; iLC < basinSettings.GetLandCoverCount(); ++iLC) {
            LandCoverSettings lc = basinSettings.GetLandCoverSettings(iLC);
            if (!NearlyZero(lc.fraction, PRECISION)) {
                present.insert(lc.name);
            }
        }

        // Prefer the variant whose land-cover set matches the unit's covers exactly.
        int matchedId = -1;
        for (int id = 1; id <= structureCount; ++id) {
            if (structureCovers[id] == present) {
                matchedId = id;
                break;
            }
        }

        // No exact match: use the smallest variant whose covers are a superset of the
        // unit's present covers (so the unit keeps all its covers, with at most a few
        // zero-area extras rather than a missing one). Fall back to the largest variant
        // if none is a superset.
        if (matchedId < 0) {
            int smallestSuperset = -1;
            size_t supersetSize = std::numeric_limits<size_t>::max();
            int largest = 1;
            size_t largestSize = 0;
            for (int id = 1; id <= structureCount; ++id) {
                const std::set<string>& covers = structureCovers[id];
                if (covers.size() > largestSize) {
                    largest = id;
                    largestSize = covers.size();
                }
                bool isSuperset = std::includes(covers.begin(), covers.end(), present.begin(), present.end());
                if (isSuperset && covers.size() < supersetSize) {
                    smallestSuperset = id;
                    supersetSize = covers.size();
                }
            }
            matchedId = (smallestSuperset >= 0) ? smallestSuperset : largest;
        }

        _subBasin->GetHydroUnit(iUnit)->SetStructureId(matchedId);
    }
}

void ModelBuilder::BuildModelStructure(SettingsModel& modelSettings) {
    // The sub-basin is catchment-level and built from the primary structure (1);
    // each hydro unit then builds its own assigned structure variant.
    modelSettings.SelectStructure(1);

    CreateSubBasinComponents(modelSettings);
    CreateHydroUnitsComponents(modelSettings);
}

void ModelBuilder::CreateSubBasinComponents(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBrickCount(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        const BrickSettings& brickSettings = modelSettings.GetSubBasinBrickSettings(iBrick);

        auto brickPtr = Brick::Factory(brickSettings);
        Brick* brick = brickPtr.get();
        brick->SetName(brickSettings.name);
        brick->SetParameters(brickSettings);
        _subBasin->AddBrick(std::move(brickPtr));

        for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            auto processPtr = Process::Factory(processSettings, brick);
            Process* process = processPtr.get();
            process->SetName(processSettings.name);
            process->SetTimeMachine(_timer);
            process->SetParameters(processSettings);
            brick->AddProcess(std::move(processPtr));

            if (processSettings.type == "overflow") {
                brick->GetWaterContainer()->LinkOverflow(process);
            }
        }
    }

    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplitterCount(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);

        auto splitterPtr = Splitter::Factory(splitterSettings);
        Splitter* splitter = splitterPtr.get();
        splitter->SetName(splitterSettings.name);
        splitter->SetParameters(splitterSettings);
        _subBasin->AddSplitter(std::move(splitterPtr));
    }

    LinkSubBasinProcessesTargetBricks(modelSettings);
    BuildSubBasinBricksFluxes(modelSettings);
    BuildSubBasinSplittersFluxes(modelSettings);
}

void ModelBuilder::CreateHydroUnitsComponents(SettingsModel& modelSettings) {
    int hydroUnitCount = _subBasin->GetHydroUnitCount();

    for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
        HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);

        // Each unit builds its assigned structure variant (defaults to structure 1).
        modelSettings.SelectStructure(unit->GetStructureId());
        vecInt surfaceCompIndices = modelSettings.GetSurfaceComponentBricksIndices();
        vecInt landCoversIndices = modelSettings.GetLandCoverBricksIndices();

        for (int iBrick : surfaceCompIndices) {
            CreateHydroUnitBrick(modelSettings, unit, iBrick);
        }

        for (int iBrick : landCoversIndices) {
            CreateHydroUnitBrick(modelSettings, unit, iBrick);
        }

        for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBrickCount(); ++iBrick) {
            if (std::find(surfaceCompIndices.begin(), surfaceCompIndices.end(), iBrick) != surfaceCompIndices.end()) {
                continue;
            }
            if (std::find(landCoversIndices.begin(), landCoversIndices.end(), iBrick) != landCoversIndices.end()) {
                continue;
            }
            CreateHydroUnitBrick(modelSettings, unit, iBrick);
        }

        for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplitterCount(); ++iSplitter) {
            modelSettings.SelectHydroUnitSplitter(iSplitter);
            const SplitterSettings& splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);

            auto splitterPtr = Splitter::Factory(splitterSettings);
            Splitter* splitter = splitterPtr.get();
            splitter->SetName(splitterSettings.name);
            splitter->SetParameters(splitterSettings);
            unit->AddSplitter(std::move(splitterPtr));

            BuildForcingConnections(splitterSettings, unit, splitter);
            splitter->SetHydroUnitProperties(unit);
        }

        LinkSurfaceComponentsParents(modelSettings, unit);
    }

    for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
        HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);
        modelSettings.SelectStructure(unit->GetStructureId());
        LinkHydroUnitProcessesTargetBricks(modelSettings, unit);
        BuildHydroUnitBricksFluxes(modelSettings, unit);
        BuildHydroUnitSplittersFluxes(modelSettings, unit);
    }
}

void ModelBuilder::CreateHydroUnitBrick(SettingsModel& modelSettings, HydroUnit* unit, int iBrick) {
    modelSettings.SelectHydroUnitBrick(iBrick);
    const BrickSettings& brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrick);

    auto brickPtr = std::unique_ptr<Brick>(Brick::Factory(brickSettings));
    Brick* brick = brickPtr.get();
    brick->SetName(brickSettings.name);
    brick->SetParameters(brickSettings);
    if (brickSettings.computedDirectly) {
        brick->SetNeedsSolver(false);
    }
    unit->AddBrick(std::move(brickPtr));

    BuildForcingConnections(brickSettings, unit, brick);

    for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
        modelSettings.SelectProcess(iProcess);
        const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

        auto processPtr = std::unique_ptr<Process>(Process::Factory(processSettings, brick));
        Process* process = processPtr.get();
        process->SetName(processSettings.name);
        process->SetTimeMachine(_timer);
        process->SetHydroUnitProperties(unit, brick);
        process->SetParameters(processSettings);
        brick->AddProcess(std::move(processPtr));

        if (processSettings.type == "overflow") {
            brick->GetWaterContainer()->LinkOverflow(process);
        }

        BuildForcingConnections(processSettings, unit, process);
    }
}

void ModelBuilder::UpdateSubBasinParameters(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBrickCount(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        const BrickSettings& brickSettings = modelSettings.GetSubBasinBrickSettings(iBrick);
        Brick* brick = _subBasin->GetBrick(iBrick);
        brick->SetParameters(brickSettings);

        for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);
            Process* process = brick->GetProcess(iProcess);
            process->SetParameters(processSettings);
        }
    }

    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplitterCount(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);
        Splitter* splitter = _subBasin->GetSplitter(iSplitter);
        splitter->SetParameters(splitterSettings);
    }
}

void ModelBuilder::UpdateHydroUnitsParameters(SettingsModel& modelSettings) {
    for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitCount(); ++iUnit) {
        HydroUnit* unit = _subBasin->GetHydroUnit(iUnit);

        // Match the structure variant this unit was built with (defaults to 1).
        modelSettings.SelectStructure(unit->GetStructureId());

        for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBrickCount(); ++iBrick) {
            modelSettings.SelectHydroUnitBrick(iBrick);
            const BrickSettings& brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrick);
            Brick* brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrick).name);
            brick->SetParameters(brickSettings);

            for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
                modelSettings.SelectProcess(iProcess);
                const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);
                Process* process = brick->GetProcess(iProcess);
                process->SetParameters(processSettings);
            }
        }

        for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplitterCount(); ++iSplitter) {
            modelSettings.SelectHydroUnitSplitter(iSplitter);
            const SplitterSettings& splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);
            Splitter* splitter = unit->GetSplitter(iSplitter);
            splitter->SetParameters(splitterSettings);
        }
    }
}

void ModelBuilder::LinkSurfaceComponentsParents(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetSurfaceComponentBrickCount(); ++iBrick) {
        const BrickSettings& brickSettings = modelSettings.GetSurfaceComponentBrickSettings(iBrick);
        if (!brickSettings.parent.empty()) {
            auto surfaceComponentBrick = dynamic_cast<SurfaceComponent*>(unit->GetBrick(brickSettings.name));
            auto landCoverBrick = dynamic_cast<LandCover*>(unit->GetBrick(brickSettings.parent));
            assert(surfaceComponentBrick);
            assert(landCoverBrick);
            surfaceComponentBrick->SetParent(landCoverBrick);
        }
    }
}

void ModelBuilder::LinkSubBasinProcessesTargetBricks(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBrickCount(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = _subBasin->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            if (process->NeedsTargetBrickLinking()) {
                if (processSettings.outputs.size() != 1) {
                    throw ModelConfigError("There can only be a single process output for brick linking.");
                }
                Brick* targetBrick = _subBasin->GetBrick(processSettings.outputs[0].target);
                process->SetTargetBrick(targetBrick);
            }

            if (process->NeedsGateBrickLinking()) {
                if (processSettings.gateBrick.empty()) {
                    throw ModelConfigError(
                        std::format("The process '{}' requires a gate brick.", processSettings.name));
                }
                process->SetGateBrick(_subBasin->GetBrick(processSettings.gateBrick));
            }
        }
    }
}

void ModelBuilder::LinkHydroUnitProcessesTargetBricks(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBrickCount(); ++iBrick) {
        modelSettings.SelectHydroUnitBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrick).name);
            Process* process = brick->GetProcess(iProcess);

            if (process->NeedsTargetBrickLinking()) {
                if (process->LinksMultipleTargets()) {
                    for (const auto& output : processSettings.outputs) {
                        Brick* targetBrick = nullptr;
                        if (unit->HasBrick(output.target)) {
                            targetBrick = unit->GetBrick(output.target);
                        } else {
                            targetBrick = _subBasin->GetBrick(output.target);
                        }
                        process->AddTargetBrickWithWeights(targetBrick,
                                                           FindLandCoversFeeding(output.target, unit, modelSettings));
                    }
                } else {
                    if (processSettings.outputs.size() != 1) {
                        throw ModelConfigError("There can only be a single process output for brick linking.");
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

            if (process->NeedsGateBrickLinking()) {
                if (processSettings.gateBrick.empty()) {
                    throw ModelConfigError(
                        std::format("The process '{}' requires a gate brick.", processSettings.name));
                }
                Brick* gateBrick = nullptr;
                if (unit->HasBrick(processSettings.gateBrick)) {
                    gateBrick = unit->GetBrick(processSettings.gateBrick);
                } else {
                    gateBrick = _subBasin->GetBrick(processSettings.gateBrick);
                }
                process->SetGateBrick(gateBrick);
            }
        }
    }
}

std::vector<Brick*> ModelBuilder::FindLandCoversFeeding(const string& targetName, HydroUnit* unit,
                                                        SettingsModel& modelSettings) {
    // Find the land cover bricks whose processes send water to the given target
    // brick (e.g. the soil moisture store). Their area fractions weight the
    // capillary flux fanned back to that target. Uses the currently selected
    // structure (set per unit by the caller).
    std::vector<Brick*> covers;
    for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBrickCount(); ++iBrick) {
        const BrickSettings& brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrick);
        if (!unit->HasBrick(brickSettings.name)) {
            continue;
        }
        Brick* brick = unit->GetBrick(brickSettings.name);
        if (!brick->CanHaveAreaFraction()) {
            continue;  // only land covers carry an area fraction
        }
        for (const auto& process : brickSettings.processes) {
            for (const auto& output : process.outputs) {
                if (output.target == targetName) {
                    covers.push_back(brick);
                }
            }
        }
    }

    return covers;
}

void ModelBuilder::BuildSubBasinBricksFluxes(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubBasinBrickCount(); ++iBrick) {
        modelSettings.SelectSubBasinBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = _subBasin->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            if (process->ToAtmosphere()) {
                auto fluxPtr = std::make_unique<FluxToAtmosphere>();
                process->AttachFluxOut(std::move(fluxPtr));
                continue;
            }

            for (const auto& output : processSettings.outputs) {
                std::unique_ptr<Flux> fluxPtr;
                Flux* flux;

                if (output.target == "outlet") {
                    fluxPtr = std::make_unique<FluxToOutlet>();
                    flux = fluxPtr.get();
                    flux->SetType(output.fluxType);
                    _subBasin->AttachOutletFlux(flux);

                } else if (_subBasin->HasBrick(output.target)) {
                    Brick* targetBrick = _subBasin->GetBrick(output.target);
                    if (output.isInstantaneous) {
                        fluxPtr = std::make_unique<FluxToBrickInstantaneous>(targetBrick);
                    } else {
                        fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                    }
                    flux = fluxPtr.get();
                    flux->SetType(output.fluxType);
                    targetBrick->AttachFluxIn(flux);

                } else if (_subBasin->HasSplitter(output.target)) {
                    Splitter* targetSplitter = _subBasin->GetSplitter(output.target);
                    fluxPtr = std::make_unique<FluxSimple>();
                    flux = fluxPtr.get();
                    flux->SetType(output.fluxType);
                    flux->SetAsStatic();
                    targetSplitter->AttachFluxIn(flux);

                } else {
                    throw ModelConfigError(std::format("The target {} to attach the flux was no found", output.target));
                }

                process->AttachFluxOut(std::move(fluxPtr));
            }
        }
    }
}

void ModelBuilder::BuildHydroUnitBricksFluxes(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBrickCount(); ++iBrick) {
        modelSettings.SelectHydroUnitBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrick).name);
            Process* process = brick->GetProcess(iProcess);

            if (process->ToAtmosphere()) {
                auto fluxPtr = std::make_unique<FluxToAtmosphere>();
                process->AttachFluxOut(std::move(fluxPtr));
                continue;
            }

            for (const auto& output : processSettings.outputs) {
                if (output.target == "outlet") {
                    auto fluxPtr = std::make_unique<FluxToOutlet>();
                    Flux* flux = fluxPtr.get();
                    flux->SetAsStatic();
                    flux->SetType(output.fluxType);
                    flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());
                    if (brick->CanHaveAreaFraction()) {
                        flux->NeedsWeighting(true);
                    }
                    _subBasin->AttachOutletFlux(flux);
                    process->AttachFluxOut(std::move(fluxPtr));

                } else if (output.target == "lateral") {
                    assert(process->IsLateralProcess());
                    auto lateralProcess = dynamic_cast<ProcessLateral*>(process);

                    for (auto& lateralConnection : unit->GetLateralConnections()) {
                        HydroUnit* receiver = lateralConnection->GetReceiver();

                        if (output.fluxType == ContentType::Snow) {
                            for (auto& targetBrick : receiver->GetSnowpacks()) {
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
                                flux->NeedsWeighting(true);
                                targetBrick->AttachFluxIn(flux);
                                lateralProcess->AttachFluxOutWithWeight(std::move(fluxPtr),
                                                                        lateralConnection->GetFraction());
                            }
                        } else {
                            throw NotImplemented(
                                std::format("ModelBuilder::BuildHydroUnitBricksFluxes - Lateral flux type {} "
                                            "not yet supported",
                                            static_cast<int>(output.fluxType)));
                        }
                    }

                } else if (unit->HasBrick(output.target) || _subBasin->HasBrick(output.target)) {
                    bool toSubBasin = false;
                    Brick* targetBrick = nullptr;

                    if (unit->HasBrick(output.target)) {
                        targetBrick = unit->GetBrick(output.target);
                    } else {
                        targetBrick = _subBasin->GetBrick(output.target);
                        toSubBasin = true;
                    }

                    std::unique_ptr<Flux> fluxPtr;
                    if (output.isInstantaneous) {
                        fluxPtr = std::make_unique<FluxToBrickInstantaneous>(targetBrick);
                    } else {
                        fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                    }

                    Flux* flux = fluxPtr.get();
                    flux->SetType(output.fluxType);

                    if (brick->CanHaveAreaFraction() && !targetBrick->CanHaveAreaFraction()) {
                        flux->NeedsWeighting(true);
                    }

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

                    if (unit->HasSplitter(output.target)) {
                        targetSplitter = unit->GetSplitter(output.target);
                    } else {
                        targetSplitter = _subBasin->GetSplitter(output.target);
                        toSubBasin = true;
                    }

                    auto fluxPtr = std::make_unique<FluxSimple>();
                    Flux* flux = fluxPtr.get();
                    flux->SetType(output.fluxType);
                    flux->SetAsStatic();

                    if (brick->CanHaveAreaFraction()) {
                        flux->NeedsWeighting(true);
                    }

                    if (toSubBasin) {
                        flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());
                    }

                    targetSplitter->AttachFluxIn(flux);
                    process->AttachFluxOut(std::move(fluxPtr));

                } else {
                    throw ModelConfigError(std::format("The target {} to attach the flux was no found", output.target));
                }
            }
        }
    }
}

void ModelBuilder::BuildSubBasinSplittersFluxes(SettingsModel& modelSettings) {
    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplitterCount(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);

        Splitter* splitter = _subBasin->GetSplitter(iSplitter);

        for (const auto& output : splitterSettings.outputs) {
            std::unique_ptr<Flux> fluxPtr;
            Flux* flux;

            if (output.target == "outlet") {
                fluxPtr = std::make_unique<FluxToOutlet>();
                flux = fluxPtr.get();
                flux->SetType(output.fluxType);
                _subBasin->AttachOutletFlux(flux);

            } else if (_subBasin->HasBrick(output.target)) {
                Brick* targetBrick = _subBasin->GetBrick(output.target);
                fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                flux = fluxPtr.get();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);
                targetBrick->AttachFluxIn(flux);

            } else if (_subBasin->HasSplitter(output.target)) {
                Splitter* targetSplitter = _subBasin->GetSplitter(output.target);
                fluxPtr = std::make_unique<FluxSimple>();
                flux = fluxPtr.get();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);
                targetSplitter->AttachFluxIn(flux);

            } else {
                throw ModelConfigError(std::format("The target {} to attach the flux was no found", output.target));
            }

            splitter->AttachFluxOut(std::move(fluxPtr));
        }
    }
}

void ModelBuilder::BuildHydroUnitSplittersFluxes(SettingsModel& modelSettings, HydroUnit* unit) {
    for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplitterCount(); ++iSplitter) {
        modelSettings.SelectHydroUnitSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);

        Splitter* splitter = unit->GetSplitter(iSplitter);

        for (const auto& output : splitterSettings.outputs) {
            std::unique_ptr<Flux> fluxPtr;
            Flux* flux;

            if (output.target == "outlet") {
                fluxPtr = std::make_unique<FluxToOutlet>();
                flux = fluxPtr.get();
                flux->SetType(output.fluxType);
                flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());
                _subBasin->AttachOutletFlux(flux);

            } else if (unit->HasBrick(output.target) || _subBasin->HasBrick(output.target)) {
                bool toSubBasin = false;
                Brick* targetBrick = nullptr;

                if (unit->HasBrick(output.target)) {
                    targetBrick = unit->GetBrick(output.target);
                } else {
                    targetBrick = _subBasin->GetBrick(output.target);
                    toSubBasin = true;
                }

                fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                flux = fluxPtr.get();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);

                if (toSubBasin) {
                    flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());
                }

                targetBrick->AttachFluxIn(flux);

            } else if (unit->HasSplitter(output.target) || _subBasin->HasSplitter(output.target)) {
                bool toSubBasin = false;
                Splitter* targetSplitter = nullptr;

                if (unit->HasSplitter(output.target)) {
                    targetSplitter = unit->GetSplitter(output.target);
                } else {
                    targetSplitter = _subBasin->GetSplitter(output.target);
                    toSubBasin = true;
                }

                fluxPtr = std::make_unique<FluxSimple>();
                flux = fluxPtr.get();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);

                if (toSubBasin) {
                    flux->SetFractionUnitArea(unit->GetArea() / _subBasin->GetArea());
                }

                targetSplitter->AttachFluxIn(flux);

            } else {
                throw ModelConfigError(std::format("The target {} to attach the flux was no found", output.target));
            }

            splitter->AttachFluxOut(std::move(fluxPtr));
        }
    }
}

void ModelBuilder::BuildForcingConnections(const BrickSettings& brickSettings, HydroUnit* unit, Brick* brick) {
    for (auto forcingType : brickSettings.forcing) {
        if (!unit->HasForcing(forcingType)) {
            unit->AddForcing(std::make_unique<Forcing>(forcingType));
        }

        auto forcing = unit->GetForcing(forcingType);
        auto forcingFlux = std::make_unique<FluxForcing>();
        forcingFlux->AttachForcing(forcing);
        brick->AttachFluxIn(std::move(forcingFlux));
    }
}

void ModelBuilder::BuildForcingConnections(const ProcessSettings& processSettings, HydroUnit* unit, Process* process) {
    for (auto forcingType : processSettings.forcing) {
        if (!unit->HasForcing(forcingType)) {
            unit->AddForcing(std::make_unique<Forcing>(forcingType));
        }

        auto forcing = unit->GetForcing(forcingType);
        process->AttachForcing(forcing);
    }
}

void ModelBuilder::BuildForcingConnections(const SplitterSettings& splitterSettings, HydroUnit* unit,
                                           Splitter* splitter) {
    for (auto forcingType : splitterSettings.forcing) {
        if (!unit->HasForcing(forcingType)) {
            unit->AddForcing(std::make_unique<Forcing>(forcingType));
        }

        auto forcing = unit->GetForcing(forcingType);
        splitter->AttachForcing(forcing);
    }
}

void ModelBuilder::ConnectLoggerToValues(SettingsModel& modelSettings) {
    double* valPt = nullptr;

    // Sub basin values. The sub-basin is catchment-level and built from the primary
    // structure (1); select it so the positional indices below match its labels.
    modelSettings.SelectStructure(1);
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
                throw ShouldNotHappen(
                    std::format("ModelBuilder::ConnectLoggerToValues - Log item '{}' not found in sub-basin brick {}",
                                logItem, iBrickType));
            }
            _logger->SetSubBasinValuePointer(iLabel, valPt);
            iLabel++;
        }

        for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            for (const auto& logItem : processSettings.logItems) {
                Process* process = _subBasin->GetBrick(iBrickType)->GetProcess(iProcess);
                valPt = process->GetValuePointer(logItem);
                if (valPt == nullptr) {
                    throw ShouldNotHappen(
                        std::format("ModelBuilder::ConnectLoggerToValues - Log item '{}' not found in sub-basin "
                                    "process {} of brick {}",
                                    logItem, iProcess, iBrickType));
                }
                _logger->SetSubBasinValuePointer(iLabel, valPt);
                if (logItem == "output" && process->ToAtmosphere()) {
                    _logger->AddSubBasinEtIndex(iLabel);
                }
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
                throw ShouldNotHappen(
                    std::format("ModelBuilder::ConnectLoggerToValues - Log item '{}' not found in sub-basin "
                                "splitter {}",
                                logItem, iSplitter));
            }
            _logger->SetSubBasinValuePointer(iLabel, valPt);
            iLabel++;
        }
    }

    vecStr genericLogLabels = modelSettings.GetSubBasinGenericLogLabels();
    for (auto& genericLogLabel : genericLogLabels) {
        valPt = _subBasin->GetValuePointer(genericLogLabel);
        if (valPt == nullptr) {
            throw ShouldNotHappen(
                std::format("ModelBuilder::ConnectLoggerToValues - Generic log label '{}' not found in sub-basin",
                            genericLogLabel));
        }
        _logger->SetSubBasinValuePointer(iLabel, valPt);
        iLabel++;
    }

    // Hydro unit values. Labels are the union across structure variants; each unit
    // connects only the labels its own structure provides (others stay NaN). The
    // value pointers are therefore keyed by label string, not by position.
    vecStr hydroUnitLabels = modelSettings.GetHydroUnitLogLabels();
    std::map<string, int> labelIndex;
    for (int i = 0; i < static_cast<int>(hydroUnitLabels.size()); ++i) {
        labelIndex[hydroUnitLabels[i]] = i;
    }
    std::set<int> etIndicesSeen;  // an ET label must be registered once, not once per unit

    for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitCount(); ++iUnit) {
        auto unit = _subBasin->GetHydroUnit(iUnit);
        modelSettings.SelectStructure(unit->GetStructureId());

        for (int iBrickType = 0; iBrickType < modelSettings.GetHydroUnitBrickCount(); ++iBrickType) {
            modelSettings.SelectHydroUnitBrick(iBrickType);
            const BrickSettings& brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrickType);
            Brick* brick = unit->GetBrick(brickSettings.name);

            for (const auto& logItem : brickSettings.logItems) {
                valPt = brick->GetBaseValuePointer(logItem);
                if (valPt == nullptr) {
                    valPt = brick->GetValuePointer(logItem);
                }
                if (valPt == nullptr) {
                    throw ShouldNotHappen(
                        std::format("ModelBuilder::ConnectLoggerToValues - Log item '{}' not found in hydro unit "
                                    "{} brick {}",
                                    logItem, iUnit, brickSettings.name));
                }
                _logger->SetHydroUnitValuePointer(iUnit, labelIndex.at(brickSettings.name + ":" + logItem), valPt);
            }

            for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
                modelSettings.SelectProcess(iProcess);
                const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

                for (const auto& logItem : processSettings.logItems) {
                    Process* process = brick->GetProcess(iProcess);
                    valPt = process->GetValuePointer(logItem);
                    if (valPt == nullptr) {
                        throw ShouldNotHappen(
                            std::format("ModelBuilder::ConnectLoggerToValues - Log item '{}' not found in hydro "
                                        "unit {} process {} of brick {}",
                                        logItem, iUnit, processSettings.name, brickSettings.name));
                    }
                    int idx = labelIndex.at(brickSettings.name + ":" + processSettings.name + ":" + logItem);
                    _logger->SetHydroUnitValuePointer(iUnit, idx, valPt);
                    if (logItem == "output" && process->ToAtmosphere() && etIndicesSeen.insert(idx).second) {
                        _logger->AddHydroUnitEtIndex(idx);
                    }
                }
            }
        }

        for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplitterCount(); ++iSplitter) {
            modelSettings.SelectHydroUnitSplitter(iSplitter);
            const SplitterSettings& splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);

            for (const auto& logItem : splitterSettings.logItems) {
                valPt = unit->GetSplitter(iSplitter)->GetValuePointer(logItem);
                if (valPt == nullptr) {
                    throw ShouldNotHappen(
                        std::format("ModelBuilder::ConnectLoggerToValues - Log item '{}' not found in hydro unit "
                                    "{} splitter {}",
                                    logItem, iUnit, splitterSettings.name));
                }
                _logger->SetHydroUnitValuePointer(iUnit, labelIndex.at(splitterSettings.name + ":" + logItem), valPt);
            }
        }
    }

    // Fractions. Same per-unit, union-keyed approach as the hydro-unit values.
    vecStr fractionLabels = modelSettings.GetLandCoverBricksNames();
    std::map<string, int> fractionIndex;
    for (int i = 0; i < static_cast<int>(fractionLabels.size()); ++i) {
        fractionIndex[fractionLabels[i]] = i;
    }

    for (int iUnit = 0; iUnit < _subBasin->GetHydroUnitCount(); ++iUnit) {
        auto unit = _subBasin->GetHydroUnit(iUnit);
        modelSettings.SelectStructure(unit->GetStructureId());

        for (int iBrickType : modelSettings.GetLandCoverBricksIndices()) {
            modelSettings.SelectHydroUnitBrick(iBrickType);
            const BrickSettings& brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrickType);
            LandCover* brick = dynamic_cast<LandCover*>(unit->GetBrick(brickSettings.name));
            valPt = brick->GetAreaFractionPointer();

            if (valPt == nullptr) {
                throw ShouldNotHappen(
                    std::format("ModelBuilder::ConnectLoggerToValues - Area fraction pointer not found for land "
                                "cover brick '{}' in unit {}",
                                brickSettings.name, iUnit));
            }
            _logger->SetHydroUnitFractionPointer(iUnit, fractionIndex.at(brickSettings.name), valPt);
        }
    }
}
