#include "ModelHydro.h"

#include "FluxForcing.h"
#include "FluxSimple.h"
#include "FluxToAtmosphere.h"
#include "FluxToBrick.h"
#include "FluxToBrickInstantaneous.h"
#include "FluxToOutlet.h"
#include "Includes.h"
#include "LandCover.h"
#include "SurfaceComponent.h"

ModelHydro::ModelHydro(SubBasin* subBasin)
    : m_subBasin(subBasin) {
    m_processor.SetModel(this);
}

ModelHydro::~ModelHydro() {}

bool ModelHydro::InitializeWithBasin(SettingsModel& modelSettings, SettingsBasin& basinSettings) {
    wxDELETE(m_subBasin);
    m_subBasin = new SubBasin();
    if (!m_subBasin->Initialize(basinSettings)) {
        return false;
    }
    if (!Initialize(modelSettings)) {
        return false;
    }
    if (!m_subBasin->AssignFractions(basinSettings)) {
        return false;
    }

    return true;
}

bool ModelHydro::Initialize(SettingsModel& modelSettings) {
    try {
        BuildModelStructure(modelSettings);

        m_timer.Initialize(modelSettings.GetTimerSettings());
        g_timeStepInDays = *m_timer.GetTimeStepPointer();
        m_processor.Initialize(modelSettings.GetSolverSettings());
        m_logger.InitContainer(m_timer.GetTimeStepsNb(), m_subBasin->GetHydroUnitsIds(),
                               modelSettings.GetSubBasinLogLabels(), modelSettings.GetHydroUnitLogLabels());
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
        brick->AssignParameters(brickSettings);
        m_subBasin->AddBrick(brick);

        // Create the processes
        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);

            Process* process = Process::Factory(processSettings, brick);
            process->SetName(processSettings.name);
            brick->AddProcess(process);

            if (processSettings.type == "Overflow") {
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

        if (processSettings.type == "Overflow") {
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
        Brick* brick = m_subBasin->GetBrick(iBrick);
        brick->AssignParameters(brickSettings);

        // Update the processes
        for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
            modelSettings.SelectProcess(iProcess);
            ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);
            Process* process = brick->GetProcess(iProcess);
            process->AssignParameters(processSettings);
        }
    }

    // Update the splitters
    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplittersNb(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);
        Splitter* splitter = m_subBasin->GetSplitter(iSplitter);
        splitter->AssignParameters(splitterSettings);
    }
}

void ModelHydro::UpdateHydroUnitsParameters(SettingsModel& modelSettings) {
    for (int iUnit = 0; iUnit < m_subBasin->GetHydroUnitsNb(); ++iUnit) {
        HydroUnit* unit = m_subBasin->GetHydroUnit(iUnit);

        // Update the bricks for the hydro unit
        for (int iBrick = 0; iBrick < modelSettings.GetHydroUnitBricksNb(); ++iBrick) {
            modelSettings.SelectHydroUnitBrick(iBrick);
            BrickSettings brickSettings = modelSettings.GetHydroUnitBrickSettings(iBrick);
            Brick* brick = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrick).name);
            brick->AssignParameters(brickSettings);

            // Update the processes
            for (int iProcess = 0; iProcess < modelSettings.GetProcessesNb(); ++iProcess) {
                modelSettings.SelectProcess(iProcess);
                ProcessSettings processSettings = modelSettings.GetProcessSettings(iProcess);
                Process* process = brick->GetProcess(iProcess);
                process->AssignParameters(processSettings);
            }
        }

        // Update the splitters
        for (int iSplitter = 0; iSplitter < modelSettings.GetHydroUnitSplittersNb(); ++iSplitter) {
            modelSettings.SelectHydroUnitSplitter(iSplitter);
            SplitterSettings splitterSettings = modelSettings.GetHydroUnitSplitterSettings(iSplitter);
            Splitter* splitter = unit->GetSplitter(iSplitter);
            splitter->AssignParameters(splitterSettings);
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

            for (const auto& output : processSettings.outputs) {
                if (output.target == "outlet") {
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
                    flux->MultiplyFraction(unit->GetArea() / m_subBasin->GetArea());

                    // Weight by surface area
                    if (brick->CanHaveAreaFraction()) {
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

                    // Create the flux
                    if (output.instantaneous) {
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
                        flux->MultiplyFraction(unit->GetArea() / m_subBasin->GetArea());
                        flux->SetAsStatic();
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
                    if (brick->CanHaveAreaFraction()) {
                        flux->NeedsWeighting(true);
                    }

                    // From hydro unit to basin: weight by hydro unit area
                    if (toSubBasin) {
                        flux->MultiplyFraction(unit->GetArea() / m_subBasin->GetArea());
                    }

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

void ModelHydro::BuildSubBasinSplittersFluxes(SettingsModel& modelSettings) {
    for (int iSplitter = 0; iSplitter < modelSettings.GetSubBasinSplittersNb(); ++iSplitter) {
        modelSettings.SelectSubBasinSplitter(iSplitter);
        SplitterSettings splitterSettings = modelSettings.GetSubBasinSplitterSettings(iSplitter);

        Splitter* splitter = m_subBasin->GetSplitter(iSplitter);

        for (const auto& output : splitterSettings.outputs) {
            Flux* flux;
            if (output.target == "outlet") {
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
                    flux->MultiplyFraction(unit->GetArea() / m_subBasin->GetArea());  // From hydro unit to basin
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
                    flux->MultiplyFraction(unit->GetArea() / m_subBasin->GetArea());  // From hydro unit to basin
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
    for (auto& genericLogLabel : genericLogLabels) {
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
                valPt = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrickType).name)
                            ->GetBaseValuePointer(logItem);
                if (valPt == nullptr) {
                    valPt = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrickType).name)
                                ->GetValuePointer(logItem);
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
                    valPt = unit->GetBrick(modelSettings.GetHydroUnitBrickSettings(iBrickType).name)
                                ->GetProcess(iProcess)
                                ->GetValuePointer(logItem);
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

bool ModelHydro::ForcingLoaded() {
    return !m_timeSeries.empty();
}

bool ModelHydro::Run() {
    wxLogDebug(_("Initializing time series."));
    if (!InitializeTimeSeries()) {
        return false;
    }
    wxLogDebug(_("Starting the computations."));
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

void ModelHydro::Reset() {
    m_timer.Reset();
    m_logger.Reset();
    m_subBasin->Reset();
}

void ModelHydro::SaveAsInitialState() {
    m_subBasin->SaveAsInitialState();
}

bool ModelHydro::DumpOutputs(const std::string& path) {
    return m_logger.DumpOutputs(path);
}

axd ModelHydro::GetOutletDischarge() {
    return m_logger.GetOutletDischarge();
}

bool ModelHydro::AddTimeSeries(TimeSeries* timeSeries) {
    for (auto ts : m_timeSeries) {
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

bool ModelHydro::CreateTimeSeries(const std::string& varName, const axd& time, const axi& ids, const axxd& data) {
    TimeSeries* timeSeries = TimeSeries::Create(varName, time, ids, data);
    if (!AddTimeSeries(timeSeries)) {
        return false;
    }
    return true;
}

void ModelHydro::ClearTimeSeries() {
    for (auto ts : m_timeSeries) {
        wxDELETE(ts);
    }
    m_timeSeries.resize(0);
}

bool ModelHydro::AttachTimeSeriesToHydroUnits() {
    wxASSERT(m_subBasin);

    for (auto timeSeries : m_timeSeries) {
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
    for (auto timeSeries : m_timeSeries) {
        wxASSERT(timeSeries);
        if (!timeSeries->SetCursorToDate(m_timer.GetDate())) {
            return false;
        }
    }

    return true;
}

bool ModelHydro::UpdateForcing() {
    for (auto timeSeries : m_timeSeries) {
        if (!timeSeries->AdvanceOneTimeStep()) {
            return false;
        }
    }

    return true;
}
