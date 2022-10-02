#include "SubBasin.h"

#include "SurfaceComponent.h"

SubBasin::SubBasin() : m_area(0), m_outletTotal(0), m_needsCleanup(false) {}

SubBasin::~SubBasin() {
    if (m_needsCleanup) {
        for (auto& hydroUnit : m_hydroUnits) {
            wxDELETE(hydroUnit);
        }
    }
}

bool SubBasin::Initialize(SettingsBasin& basinSettings) {
    try {
        BuildBasin(basinSettings);
    } catch (const std::exception& e) {
        wxLogError(_("An exception occurred during basin initialization: %s."), e.what());
        return false;
    }

    return true;
}

void SubBasin::BuildBasin(SettingsBasin& basinSettings) {
    m_needsCleanup = true;
    for (int iUnit = 0; iUnit < basinSettings.GetHydroUnitsNb(); ++iUnit) {
        basinSettings.SelectUnit(iUnit);

        HydroUnitSettings unitSettings = basinSettings.GetHydroUnitSettings(iUnit);
        auto unit = new HydroUnit(unitSettings.area);
        unit->SetId(unitSettings.id);
        AddHydroUnit(unit);
    }
}

bool SubBasin::AssignFractions(SettingsBasin& basinSettings) {
    try {
        for (int iUnit = 0; iUnit < basinSettings.GetHydroUnitsNb(); ++iUnit) {
            basinSettings.SelectUnit(iUnit);

            for (int iElement = 0; iElement < basinSettings.GetSurfaceElementsNb(); ++iElement) {
                SurfaceElementSettings elementSettings = basinSettings.GetSurfaceElementSettings(iElement);

                auto brick = dynamic_cast<SurfaceComponent*>(m_hydroUnits[iUnit]->GetBrick(elementSettings.name));
                brick->SetAreaFraction(elementSettings.fraction);
            }
        }
    } catch (const std::exception& e) {
        wxLogError(_("An exception occurred while assigning the fractions: %s."), e.what());
        return false;
    }

    return true;
}

bool SubBasin::IsOk() {
    if (m_hydroUnits.empty()) {
        wxLogError(_("The sub basin has no hydro unit attached."));
        return false;
    }
    for (auto unit : m_hydroUnits) {
        if (!unit->IsOk()) return false;
    }
    for (auto brick : m_bricks) {
        if (!brick->IsOk()) return false;
    }
    for (auto splitter : m_splitters) {
        if (!splitter->IsOk()) return false;
    }

    return true;
}

void SubBasin::AddBrick(Brick* brick) {
    wxASSERT(brick);
    m_bricks.push_back(brick);
}

void SubBasin::AddSplitter(Splitter* splitter) {
    wxASSERT(splitter);
    m_splitters.push_back(splitter);
}

void SubBasin::AddHydroUnit(HydroUnit* unit) {
    m_hydroUnits.push_back(unit);
    m_area += unit->GetArea();
}

int SubBasin::GetHydroUnitsNb() {
    return int(m_hydroUnits.size());
}

HydroUnit* SubBasin::GetHydroUnit(int index) {
    wxASSERT(m_hydroUnits.size() > index);
    wxASSERT(m_hydroUnits[index]);

    return m_hydroUnits[index];
}

vecInt SubBasin::GetHydroUnitsIds() {
    vecInt ids;
    ids.reserve(m_hydroUnits.size());
    for (auto unit : m_hydroUnits) {
        ids.push_back(unit->GetId());
    }

    return ids;
}

int SubBasin::GetBricksCount() {
    return int(m_bricks.size());
}

int SubBasin::GetSplittersCount() {
    return int(m_splitters.size());
}

Brick* SubBasin::GetBrick(int index) {
    wxASSERT(m_bricks.size() > index);
    wxASSERT(m_bricks[index]);

    return m_bricks[index];
}

bool SubBasin::HasBrick(const std::string& name) {
    for (auto brick : m_bricks) {
        if (brick->GetName() == name) {
            return true;
        }
    }
    return false;
}

Brick* SubBasin::GetBrick(const std::string& name) {
    for (auto brick : m_bricks) {
        if (brick->GetName() == name) {
            return brick;
        }
    }

    throw NotFound(wxString::Format(_("No brick with the name '%s' was found."), name));
}

Splitter* SubBasin::GetSplitter(int index) {
    wxASSERT(m_splitters.size() > index);
    wxASSERT(m_splitters[index]);

    return m_splitters[index];
}

bool SubBasin::HasSplitter(const std::string& name) {
    for (auto splitter : m_splitters) {
        if (splitter->GetName() == name) {
            return true;
        }
    }
    return false;
}

Splitter* SubBasin::GetSplitter(const std::string& name) {
    for (auto splitter : m_splitters) {
        if (splitter->GetName() == name) {
            return splitter;
        }
    }

    throw NotFound(wxString::Format(_("No splitter with the name '%s' was found."), name));
}

bool SubBasin::HasIncomingFlow() {
    return !m_inConnectors.empty();
}

void SubBasin::AddInputConnector(Connector* connector) {
    wxASSERT(connector);
    m_inConnectors.push_back(connector);
}

void SubBasin::AddOutputConnector(Connector* connector) {
    wxASSERT(connector);
    m_outConnectors.push_back(connector);
}

void SubBasin::AddBehaviour(Behaviour* behaviour) {
    wxASSERT(behaviour);
    m_behaviours.push_back(behaviour);
}

void SubBasin::AttachOutletFlux(Flux* flux) {
    wxASSERT(flux);
    m_outletFluxes.push_back(flux);
}

double* SubBasin::GetValuePointer(const std::string& name) {
    if (name == "outlet") {
        return &m_outletTotal;
    }
    wxLogError(_("Element '%s' not found"), name);

    return nullptr;
}

bool SubBasin::ComputeOutletDischarge() {
    m_outletTotal = 0;
    for (auto flux : m_outletFluxes) {
        m_outletTotal += flux->GetAmount();
    }

    return true;
}
