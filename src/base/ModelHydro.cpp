#include "ModelHydro.h"

#include "FluxDirect.h"
#include "FluxForcing.h"
#include "StorageLinear.h"

ModelHydro::ModelHydro(Processor* processor, SubBasin* subBasin, TimeMachine* timer)
    : m_processor(processor),
      m_subBasin(subBasin),
      m_timer(timer)
{
    if (processor) {
        processor->SetModel(this);
    }
}

ModelHydro::~ModelHydro() {
    wxDELETE(m_processor);
    wxDELETE(m_timer);
}

ModelHydro* ModelHydro::Factory(ParameterSet &parameterSet, SubBasin* subBasin) {
    Solver* solver = Solver::Factory(parameterSet.GetSolverSettings());
    auto processor = new Processor(solver);
    auto timer = new TimeMachine(parameterSet.GetTimerSettings());
    auto model = new ModelHydro(processor, subBasin, timer);

    BuildModelStructure(parameterSet, subBasin);

    return model;
}

void ModelHydro::BuildModelStructure(ParameterSet &parameterSet, SubBasin* subBasin) {

    if (parameterSet.GetStructuresNb() > 1) {
        throw NotImplemented();
    }

    parameterSet.SelectStructure(1);

    for (int iUnit = 0; iUnit < subBasin->GetHydroUnitsNb(); ++iUnit) {
        HydroUnit* unit = subBasin->GetHydroUnit(iUnit);

        for (int iBrick = 0; iBrick < parameterSet.GetBricksNb(); ++iBrick) {
            BrickSettings brickSettings = parameterSet.GetBrickSettings(iBrick);

            Brick* brick = Brick::Factory(brickSettings, unit);
            brick->SetName(brickSettings.name);

            BuildForcingConnections(brickSettings, unit, brick);
        }

        BuildFluxes(parameterSet, subBasin, unit);
    }
}

void ModelHydro::BuildFluxes(const ParameterSet& parameterSet, SubBasin* subBasin, HydroUnit* unit) {
    for (int iBrick = 0; iBrick < parameterSet.GetBricksNb(); ++iBrick) {
        BrickSettings brickSettings = parameterSet.GetBrickSettings(iBrick);

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
    if (m_processor == nullptr) {
        wxLogError(_("The processor was not created."));
        return false;
    }
    if (m_timer == nullptr) {
        wxLogError(_("The timer was not created."));
        return false;
    }

    return true;
}

bool ModelHydro::UpdateForcing() {
    return false;
}

bool ModelHydro::Run() {
    m_processor->Initialize();
    while (!m_timer->IsOver()) {
        if (!UpdateForcing()) {
            wxLogError(_("Failed updating the forcing data."));
            return false;
        }
        if (!m_processor->ProcessTimeStep()) {
            wxLogError(_("Failed running the model."));
            return false;
        }
        m_timer->IncrementTime();
    }
    return false;
}

void ModelHydro::SetTimeSeries(TimeSeries* timeSeries) {
    wxASSERT(m_timeSeries[timeSeries->GetVariableType()] == nullptr);
    m_timeSeries[timeSeries->GetVariableType()] = timeSeries;
}