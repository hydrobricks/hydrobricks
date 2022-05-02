#include "ModelHydro.h"

#include "FluxDirect.h"
#include "FluxForcing.h"
#include "StorageLinear.h"

ModelHydro::ModelHydro(SubBasin* subBasin)
    : m_subBasin(subBasin)
{
    m_processor.SetModel(this);
}

ModelHydro::~ModelHydro() {
}

bool ModelHydro::Initialize(SettingsModel& modelSettings) {
    BuildModelStructure(modelSettings);

    m_timer.Initialize(modelSettings.GetTimerSettings());
    m_processor.Initialize(modelSettings.GetSolverSettings());
    m_logger.InitContainer(m_timer.GetTimeStepsNb(),
                           m_subBasin->GetHydroUnitsNb(),
                           modelSettings.GetAggregatedLogLabels(),
                           modelSettings.GetHydroUnitLogLabels());
    //m_logger.ConnectToValues();

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
            BrickSettings brickSettings = modelSettings.GetBrickSettings(iBrick);

            Brick* brick = Brick::Factory(brickSettings, unit);
            brick->SetName(brickSettings.name);

            BuildForcingConnections(brickSettings, unit, brick);
        }

        BuildFluxes(modelSettings, m_subBasin, unit);
    }
}

void ModelHydro::BuildFluxes(const SettingsModel& modelSettings, SubBasin* subBasin, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < modelSettings.GetBricksNb(); ++iBrick) {
        BrickSettings brickSettings = modelSettings.GetBrickSettings(iBrick);

        for (const auto& output: brickSettings.outputs)  {
            Flux* flux;
            if (output.type.IsSameAs("Direct", false)) {
                flux = new FluxDirect();
            } else {
                throw NotImplemented();
            }

            Brick* brickOut = unit->GetBrick(iBrick);
            brickOut->AttachFluxOut(flux);

            if (output.target.IsSameAs("outlet", false)) {
                subBasin->AttachOutletFlux(flux);
            } else {
                Brick* brickIn = unit->GetBrick(output.target);
                brickIn->AttachFluxIn(flux);
            }
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

bool ModelHydro::IsOk() {
    if (!m_subBasin->IsOk()) return false;

    return true;
}

bool ModelHydro::Run() {
    if (!InitializeTimeSeries()) {
        return false;
    }
    while (!m_timer.IsOver()) {
        if (!UpdateForcing()) {
            wxLogError(_("Failed updating the forcing data."));
            return false;
        }
        if (!m_processor.ProcessTimeStep()) {
            wxLogError(_("Failed running the model."));
            return false;
        }
        m_timer.IncrementTime();
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