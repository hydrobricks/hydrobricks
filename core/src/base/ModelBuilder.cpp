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
#include "Subbasin.h"
#include "SurfaceComponent.h"
#include "TimeMachine.h"

ModelBuilder::ModelBuilder(Subbasin* subbasin, TimeMachine* timer, Logger* logger, int subbasinIndex, int unitOffset)
    : _subbasin(subbasin),
      _timer(timer),
      _logger(logger),
      _subbasinIndex(subbasinIndex),
      _unitOffset(unitOffset) {
    assert(subbasin);
    assert(timer);
    assert(logger);
}

std::vector<std::set<string>> ModelBuilder::StructureLandCovers(SettingsModel& modelSettings) {
    // Land-cover name set of each structure variant (ids are 1..structureCount).
    int structureCount = modelSettings.GetStructureCount();
    int previousId = modelSettings.GetSelectedStructureId();
    std::vector<std::set<string>> structureCovers(structureCount + 1);
    for (int id = 1; id <= structureCount; ++id) {
        modelSettings.SelectStructure(id);
        vecStr covers = modelSettings.GetSelectedStructureLandCoverNames();
        structureCovers[id] = std::set<string>(covers.begin(), covers.end());
    }
    modelSettings.SelectStructure(previousId);

    return structureCovers;
}

int ModelBuilder::MatchStructure(const std::vector<std::set<string>>& structureCovers,
                                 const std::set<string>& present) {
    int structureCount = static_cast<int>(structureCovers.size()) - 1;

    // Prefer the variant whose land-cover set matches the present covers exactly.
    for (int id = 1; id <= structureCount; ++id) {
        if (structureCovers[id] == present) {
            return id;
        }
    }

    // No exact match: use the smallest variant whose covers are a superset of the present covers (so all
    // the covers are kept, with at most a few zero-area extras rather than a missing one). Fall back to the
    // largest variant if none is a superset.
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

    return (smallestSuperset >= 0) ? smallestSuperset : largest;
}

void ModelBuilder::AssignHydroUnitStructures(SettingsModel& modelSettings, SettingsBasin& basinSettings) {
    if (modelSettings.GetStructureCount() <= 1) {
        return;  // single structure: every unit keeps the default id 1
    }
    std::vector<std::set<string>> structureCovers = StructureLandCovers(modelSettings);

    // The settings list every unit of the catchment; only those of this subbasin are assigned here.
    for (int iUnit = 0; iUnit < basinSettings.GetHydroUnitCount(); ++iUnit) {
        basinSettings.SelectUnit(iUnit);
        int unitId = basinSettings.GetHydroUnitSettings(iUnit).id;
        if (!_subbasin->HasHydroUnit(unitId)) {
            continue;
        }

        // Land covers actually present in this unit (non-zero fraction).
        std::set<string> present;
        for (int iLC = 0; iLC < basinSettings.GetLandCoverCount(); ++iLC) {
            LandCoverSettings lc = basinSettings.GetLandCoverSettings(iLC);
            if (!NearlyZero(lc.fraction, PRECISION)) {
                present.insert(lc.name);
            }
        }

        _subbasin->GetHydroUnitById(unitId)->SetStructureId(MatchStructure(structureCovers, present));
    }
}

void ModelBuilder::AssignSubbasinStructure(SettingsModel& modelSettings, SettingsBasin& basinSettings) {
    if (modelSettings.GetStructureCount() <= 1) {
        return;  // single structure: the subbasin keeps the default id 1
    }
    std::vector<std::set<string>> structureCovers = StructureLandCovers(modelSettings);

    // Land covers present in any hydro unit of this subbasin (non-zero fraction).
    std::set<string> present;
    for (int iUnit = 0; iUnit < basinSettings.GetHydroUnitCount(); ++iUnit) {
        basinSettings.SelectUnit(iUnit);
        int unitId = basinSettings.GetHydroUnitSettings(iUnit).id;
        if (!_subbasin->HasHydroUnit(unitId)) {
            continue;
        }
        for (int iLC = 0; iLC < basinSettings.GetLandCoverCount(); ++iLC) {
            LandCoverSettings lc = basinSettings.GetLandCoverSettings(iLC);
            if (!NearlyZero(lc.fraction, PRECISION)) {
                present.insert(lc.name);
            }
        }
    }

    _subbasin->SetStructureId(MatchStructure(structureCovers, present));
}

void ModelBuilder::BuildModelStructure(SettingsModel& modelSettings) {
    // The subbasin-level components are built from the subbasin's assigned structure variant (the primary
    // structure unless AssignSubbasinStructure chose another); each hydro unit then builds its own.
    modelSettings.SelectStructure(_subbasin->GetStructureId());

    CreateSubbasinComponents(modelSettings);
    CreateHydroUnitsComponents(modelSettings);
}

void ModelBuilder::CreateSubbasinComponents(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubbasinBrickCount(); ++iBrick) {
        modelSettings.SelectSubbasinBrick(iBrick);
        const BrickSettings& brickSettings = modelSettings.GetSubbasinBrickSettings(iBrick);

        auto brickPtr = Brick::Factory(brickSettings);
        Brick* brick = brickPtr.get();
        brick->SetName(brickSettings.name);
        brick->SetParameters(brickSettings);
        _subbasin->AddBrick(std::move(brickPtr));

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

    for (int iSplitter = 0; iSplitter < modelSettings.GetSubbasinSplitterCount(); ++iSplitter) {
        modelSettings.SelectSubbasinSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetSubbasinSplitterSettings(iSplitter);

        auto splitterPtr = Splitter::Factory(splitterSettings);
        Splitter* splitter = splitterPtr.get();
        splitter->SetName(splitterSettings.name);
        splitter->SetParameters(splitterSettings);
        _subbasin->AddSplitter(std::move(splitterPtr));
    }

    LinkSubbasinProcessesTargetBricks(modelSettings);
    BuildSubbasinBricksFluxes(modelSettings);
    BuildSubbasinSplittersFluxes(modelSettings);
}

void ModelBuilder::CreateHydroUnitsComponents(SettingsModel& modelSettings) {
    int hydroUnitCount = _subbasin->GetHydroUnitCount();

    for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
        HydroUnit* unit = _subbasin->GetHydroUnit(iUnit);

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
        HydroUnit* unit = _subbasin->GetHydroUnit(iUnit);
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
        process->SetParentContext(unit, brick);
        process->SetHydroUnitProperties(unit, brick);
        process->SetParameters(processSettings);
        brick->AddProcess(std::move(processPtr));

        if (processSettings.type == "overflow") {
            brick->GetWaterContainer()->LinkOverflow(process);
        }

        BuildForcingConnections(processSettings, unit, process);
    }
}

void ModelBuilder::UpdateSubbasinParameters(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubbasinBrickCount(); ++iBrick) {
        modelSettings.SelectSubbasinBrick(iBrick);
        const BrickSettings& brickSettings = modelSettings.GetSubbasinBrickSettings(iBrick);
        Brick* brick = _subbasin->GetBrick(iBrick);
        brick->SetParameters(brickSettings);

        for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);
            Process* process = brick->GetProcess(iProcess);
            process->SetParameters(processSettings);
        }
    }

    for (int iSplitter = 0; iSplitter < modelSettings.GetSubbasinSplitterCount(); ++iSplitter) {
        modelSettings.SelectSubbasinSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetSubbasinSplitterSettings(iSplitter);
        Splitter* splitter = _subbasin->GetSplitter(iSplitter);
        splitter->SetParameters(splitterSettings);
    }
}

void ModelBuilder::UpdateHydroUnitsParameters(SettingsModel& modelSettings) {
    for (int iUnit = 0; iUnit < _subbasin->GetHydroUnitCount(); ++iUnit) {
        HydroUnit* unit = _subbasin->GetHydroUnit(iUnit);

        // Match the structure variant this unit was built with (defaults to 1).
        modelSettings.SelectStructure(unit->GetStructureId());

        for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBrickCount(); ++iBrick) {
            modelSettings.SelectHydroUnitBrick(iBrick);
            const BrickSettings& brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrick);
            Brick* brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrick).name);
            // Refresh per-unit (spatial) parameter overrides before (re)linking the value
            // pointers, so the brick/process read each unit's own value.
            PopulateSpatialOverrides(modelSettings, unit, brickSettings.name);
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

void ModelBuilder::PopulateSpatialOverrides(SettingsModel& modelSettings, HydroUnit* unit, const string& brickName) {
    // For each spatial binding whose component is this brick, read the unit's property and
    // store a per-unit override (keyed "<brickName>:<paramName>"). Units missing the
    // property keep the shared global value.
    for (const auto& [key, property] : modelSettings.GetSpatialParameterBindings()) {
        const string& component = key.first;
        const string& paramName = key.second;
        if (component != brickName) {
            continue;
        }
        if (!unit->HasProperty(property)) {
            continue;
        }
        unit->SetParameterOverride(brickName + ":" + paramName, unit->GetPropertyFloat(property));
    }

    // Same for the parameters that are both spatial and monthly: the unit's 12 values are
    // read from 12 properties. A unit missing any of them keeps the shared global value.
    for (const auto& [key, properties] : modelSettings.GetSpatialMonthlyParameterBindings()) {
        const string& component = key.first;
        const string& paramName = key.second;
        if (component != brickName) {
            continue;
        }
        vecFloat values;
        values.reserve(properties.size());
        for (const auto& property : properties) {
            if (!unit->HasProperty(property)) {
                break;
            }
            values.push_back(unit->GetPropertyFloat(property));
        }
        if (values.size() != properties.size()) {
            continue;
        }
        unit->SetParameterOverrideMonthly(brickName + ":" + paramName, values);
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

void ModelBuilder::LinkSubbasinProcessesTargetBricks(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubbasinBrickCount(); ++iBrick) {
        modelSettings.SelectSubbasinBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = _subbasin->GetBrick(iBrick);
            Process* process = brick->GetProcess(iProcess);

            if (process->NeedsTargetBrickLinking()) {
                if (processSettings.outputs.size() != 1) {
                    throw ModelConfigError("There can only be a single process output for brick linking.");
                }
                Brick* targetBrick = _subbasin->GetBrick(processSettings.outputs[0].target);
                process->SetTargetBrick(targetBrick);
            }

            // A gate brick is linked whenever one is declared; NeedsGateBrickLinking()
            // only tells whether the process cannot work without one.
            if (processSettings.gateBricks.empty()) {
                if (process->NeedsGateBrickLinking()) {
                    throw ModelConfigError(
                        std::format("The process '{}' requires a gate brick.", processSettings.name));
                }
            } else {
                for (const auto& gateName : processSettings.gateBricks) {
                    process->AddGateBrick(_subbasin->GetBrick(gateName));
                }
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
                            targetBrick = _subbasin->GetBrick(output.target);
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
                        targetBrick = _subbasin->GetBrick(processSettings.outputs[0].target);
                    }
                    process->SetTargetBrick(targetBrick);
                }
            }

            if (processSettings.gateBricks.empty()) {
                if (process->NeedsGateBrickLinking()) {
                    throw ModelConfigError(
                        std::format("The process '{}' requires a gate brick.", processSettings.name));
                }
            } else {
                for (const auto& gateName : processSettings.gateBricks) {
                    Brick* gateBrick = nullptr;
                    if (unit->HasBrick(gateName)) {
                        gateBrick = unit->GetBrick(gateName);
                    } else {
                        gateBrick = _subbasin->GetBrick(gateName);
                    }
                    process->AddGateBrick(gateBrick);
                }
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

void ModelBuilder::BuildSubbasinBricksFluxes(SettingsModel& modelSettings) {
    for (int iBrick = 0; iBrick < modelSettings.GetSubbasinBrickCount(); ++iBrick) {
        modelSettings.SelectSubbasinBrick(iBrick);
        for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            Brick* brick = _subbasin->GetBrick(iBrick);
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
                    _subbasin->AttachOutletFlux(flux);

                } else if (_subbasin->HasBrick(output.target)) {
                    Brick* targetBrick = _subbasin->GetBrick(output.target);
                    if (output.isInstantaneous) {
                        fluxPtr = std::make_unique<FluxToBrickInstantaneous>(targetBrick);
                    } else {
                        fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                    }
                    flux = fluxPtr.get();
                    flux->SetType(output.fluxType);
                    targetBrick->AttachFluxIn(flux);

                } else if (_subbasin->HasSplitter(output.target)) {
                    Splitter* targetSplitter = _subbasin->GetSplitter(output.target);
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
                    flux->SetFractionUnitArea(unit->GetArea() / _subbasin->GetArea());
                    if (brick->CanHaveAreaFraction()) {
                        flux->NeedsWeighting(true);
                    }
                    _subbasin->AttachOutletFlux(flux);
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

                } else if (unit->HasBrick(output.target) || _subbasin->HasBrick(output.target)) {
                    bool toSubbasin = false;
                    Brick* targetBrick = nullptr;

                    if (unit->HasBrick(output.target)) {
                        targetBrick = unit->GetBrick(output.target);
                    } else {
                        targetBrick = _subbasin->GetBrick(output.target);
                        toSubbasin = true;
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

                    if (toSubbasin) {
                        flux->SetFractionUnitArea(unit->GetArea() / _subbasin->GetArea());
                        flux->SetAsStatic();
                    } else if (output.isStatic) {
                        flux->SetAsStatic();
                    }

                    targetBrick->AttachFluxIn(flux);
                    process->AttachFluxOut(std::move(fluxPtr));

                } else if (unit->HasSplitter(output.target) || _subbasin->HasSplitter(output.target)) {
                    bool toSubbasin = false;
                    Splitter* targetSplitter = nullptr;

                    if (unit->HasSplitter(output.target)) {
                        targetSplitter = unit->GetSplitter(output.target);
                    } else {
                        targetSplitter = _subbasin->GetSplitter(output.target);
                        toSubbasin = true;
                    }

                    auto fluxPtr = std::make_unique<FluxSimple>();
                    Flux* flux = fluxPtr.get();
                    flux->SetType(output.fluxType);
                    flux->SetAsStatic();

                    if (brick->CanHaveAreaFraction()) {
                        flux->NeedsWeighting(true);
                    }

                    if (toSubbasin) {
                        flux->SetFractionUnitArea(unit->GetArea() / _subbasin->GetArea());
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

void ModelBuilder::BuildSubbasinSplittersFluxes(SettingsModel& modelSettings) {
    for (int iSplitter = 0; iSplitter < modelSettings.GetSubbasinSplitterCount(); ++iSplitter) {
        modelSettings.SelectSubbasinSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetSubbasinSplitterSettings(iSplitter);

        Splitter* splitter = _subbasin->GetSplitter(iSplitter);

        for (const auto& output : splitterSettings.outputs) {
            std::unique_ptr<Flux> fluxPtr;
            Flux* flux;

            if (output.target == "outlet") {
                fluxPtr = std::make_unique<FluxToOutlet>();
                flux = fluxPtr.get();
                flux->SetType(output.fluxType);
                _subbasin->AttachOutletFlux(flux);

            } else if (_subbasin->HasBrick(output.target)) {
                Brick* targetBrick = _subbasin->GetBrick(output.target);
                fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                flux = fluxPtr.get();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);
                targetBrick->AttachFluxIn(flux);

            } else if (_subbasin->HasSplitter(output.target)) {
                Splitter* targetSplitter = _subbasin->GetSplitter(output.target);
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
                flux->SetFractionUnitArea(unit->GetArea() / _subbasin->GetArea());
                _subbasin->AttachOutletFlux(flux);

            } else if (unit->HasBrick(output.target) || _subbasin->HasBrick(output.target)) {
                bool toSubbasin = false;
                Brick* targetBrick = nullptr;

                if (unit->HasBrick(output.target)) {
                    targetBrick = unit->GetBrick(output.target);
                } else {
                    targetBrick = _subbasin->GetBrick(output.target);
                    toSubbasin = true;
                }

                fluxPtr = std::make_unique<FluxToBrick>(targetBrick);
                flux = fluxPtr.get();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);

                if (toSubbasin) {
                    flux->SetFractionUnitArea(unit->GetArea() / _subbasin->GetArea());
                }

                targetBrick->AttachFluxIn(flux);

            } else if (unit->HasSplitter(output.target) || _subbasin->HasSplitter(output.target)) {
                bool toSubbasin = false;
                Splitter* targetSplitter = nullptr;

                if (unit->HasSplitter(output.target)) {
                    targetSplitter = unit->GetSplitter(output.target);
                } else {
                    targetSplitter = _subbasin->GetSplitter(output.target);
                    toSubbasin = true;
                }

                fluxPtr = std::make_unique<FluxSimple>();
                flux = fluxPtr.get();
                flux->SetAsStatic();
                flux->SetType(output.fluxType);

                if (toSubbasin) {
                    flux->SetFractionUnitArea(unit->GetArea() / _subbasin->GetArea());
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

    // Subbasin values. The labels are the union across structure variants; this subbasin connects only
    // the labels its own variant provides (the others stay NaN), so the value pointers are keyed by label
    // string, not by position.
    modelSettings.SelectStructure(_subbasin->GetStructureId());
    vecStr subbasinLabels = modelSettings.GetSubbasinLogLabels();
    std::map<string, int> subbasinLabelIndex;
    for (int i = 0; i < static_cast<int>(subbasinLabels.size()); ++i) {
        subbasinLabelIndex[subbasinLabels[i]] = i;
    }

    for (int iBrickType = 0; iBrickType < modelSettings.GetSubbasinBrickCount(); ++iBrickType) {
        modelSettings.SelectSubbasinBrick(iBrickType);
        const BrickSettings& brickSettings = modelSettings.GetSubbasinBrickSettings(iBrickType);

        for (const auto& logItem : brickSettings.logItems) {
            valPt = _subbasin->GetBrick(iBrickType)->GetBaseValuePointer(logItem);
            if (valPt == nullptr) {
                valPt = _subbasin->GetBrick(iBrickType)->GetValuePointer(logItem);
            }
            if (valPt == nullptr) {
                throw ShouldNotHappen(
                    std::format("ModelBuilder::ConnectLoggerToValues - Log item '{}' not found in subbasin brick {}",
                                logItem, iBrickType));
            }
            _logger->SetSubbasinValuePointer(_subbasinIndex, subbasinLabelIndex.at(brickSettings.name + ":" + logItem),
                                             valPt);
        }

        for (int iProcess = 0; iProcess < modelSettings.GetProcessCount(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            const ProcessSettings& processSettings = modelSettings.GetProcessSettings(iProcess);

            for (const auto& logItem : processSettings.logItems) {
                Process* process = _subbasin->GetBrick(iBrickType)->GetProcess(iProcess);
                valPt = process->GetValuePointer(logItem);
                if (valPt == nullptr) {
                    throw ShouldNotHappen(
                        std::format("ModelBuilder::ConnectLoggerToValues - Log item '{}' not found in subbasin "
                                    "process {} of brick {}",
                                    logItem, iProcess, iBrickType));
                }
                int idx = subbasinLabelIndex.at(brickSettings.name + ":" + processSettings.name + ":" + logItem);
                _logger->SetSubbasinValuePointer(_subbasinIndex, idx, valPt);
                if (logItem == "output" && process->ToAtmosphere()) {
                    _logger->AddSubbasinEtIndex(idx);
                }
            }
        }
    }

    for (int iSplitter = 0; iSplitter < modelSettings.GetSubbasinSplitterCount(); ++iSplitter) {
        modelSettings.SelectSubbasinSplitter(iSplitter);
        const SplitterSettings& splitterSettings = modelSettings.GetSubbasinSplitterSettings(iSplitter);

        for (const auto& logItem : splitterSettings.logItems) {
            valPt = _subbasin->GetSplitter(iSplitter)->GetValuePointer(logItem);
            if (valPt == nullptr) {
                throw ShouldNotHappen(
                    std::format("ModelBuilder::ConnectLoggerToValues - Log item '{}' not found in subbasin "
                                "splitter {}",
                                logItem, iSplitter));
            }
            _logger->SetSubbasinValuePointer(_subbasinIndex,
                                             subbasinLabelIndex.at(splitterSettings.name + ":" + logItem), valPt);
        }
    }

    vecStr genericLogLabels = modelSettings.GetSubbasinGenericLogLabels();
    for (auto& genericLogLabel : genericLogLabels) {
        valPt = _subbasin->GetValuePointer(genericLogLabel);
        if (valPt == nullptr) {
            throw ShouldNotHappen(std::format(
                "ModelBuilder::ConnectLoggerToValues - Generic log label '{}' not found in subbasin", genericLogLabel));
        }
        _logger->SetSubbasinValuePointer(_subbasinIndex, subbasinLabelIndex.at(genericLogLabel), valPt);
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

    // Area the recorded values are expressed over. A land cover and its surface components hold
    // their content over their own area; the other bricks over the whole unit. A flux is recorded
    // as delivered to its recipient: over the subbasin when it goes to the outlet or to a subbasin
    // brick, over the unit when it leaves a land cover weighted by its fraction, otherwise over the
    // area of the brick it leaves.
    auto brickBasis = [](const Brick* brick) {
        return brick->CanHaveAreaFraction() ? string("land_cover") : string("hydro_unit");
    };
    auto processOutputBasis = [&](const Brick* brick, Process* process) {
        if (process->GetOutputFluxCount() == 0) {
            return brickBasis(brick);
        }
        Flux* flux = process->GetOutputFlux(0);
        if (dynamic_cast<FluxToOutlet*>(flux) != nullptr) {
            return string("subbasin");
        }
        auto fluxToBrick = dynamic_cast<FluxToBrick*>(flux);
        if (fluxToBrick != nullptr && fluxToBrick->GetTargetBrick() != nullptr &&
            _subbasin->HasBrick(fluxToBrick->GetTargetBrick()->GetName())) {
            return string("subbasin");
        }
        if (flux->NeedsWeighting()) {
            return string("hydro_unit");
        }
        return brickBasis(brick);
    };

    for (int iUnit = 0; iUnit < _subbasin->GetHydroUnitCount(); ++iUnit) {
        auto unit = _subbasin->GetHydroUnit(iUnit);
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
                int brickLabel = labelIndex.at(brickSettings.name + ":" + logItem);
                _logger->SetHydroUnitValuePointer(_unitOffset + iUnit, brickLabel, valPt);
                _logger->SetHydroUnitLabelBasis(brickLabel, brickBasis(brick));
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
                    _logger->SetHydroUnitValuePointer(_unitOffset + iUnit, idx, valPt);
                    _logger->SetHydroUnitLabelBasis(
                        idx, logItem == "output" ? processOutputBasis(brick, process) : brickBasis(brick));
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
                _logger->SetHydroUnitValuePointer(_unitOffset + iUnit,
                                                  labelIndex.at(splitterSettings.name + ":" + logItem), valPt);
            }
        }
    }

    // Fractions. Same per-unit, union-keyed approach as the hydro-unit values.
    vecStr fractionLabels = modelSettings.GetLandCoverBricksNames();
    std::map<string, int> fractionIndex;
    for (int i = 0; i < static_cast<int>(fractionLabels.size()); ++i) {
        fractionIndex[fractionLabels[i]] = i;
    }

    for (int iUnit = 0; iUnit < _subbasin->GetHydroUnitCount(); ++iUnit) {
        auto unit = _subbasin->GetHydroUnit(iUnit);
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
            _logger->SetHydroUnitFractionPointer(_unitOffset + iUnit, fractionIndex.at(brickSettings.name), valPt);
        }
    }
}
