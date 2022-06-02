#ifndef HYDROBRICKS_SUBBASIN_H
#define HYDROBRICKS_SUBBASIN_H

#include "Behaviour.h"
#include "Connector.h"
#include "HydroUnit.h"
#include "Includes.h"
#include "TimeMachine.h"
#include "SettingsBasin.h"

class SubBasin : public wxObject {
  public:
    SubBasin();

    ~SubBasin() override;

    bool Initialize(SettingsBasin& basinSettings);

    void BuildBasin(SettingsBasin& basinSettings);

    bool AssignFractions(SettingsBasin& basinSettings);

    bool IsOk();

    void AddHydroUnit(HydroUnit* unit);

    int GetHydroUnitsNb();

    HydroUnit* GetHydroUnit(int index);

    bool HasIncomingFlow();

    void AddInputConnector(Connector* connector);

    void AddOutputConnector(Connector* connector);

    void AddBehaviour(Behaviour* behaviour);

    void AttachOutletFlux(Flux* pFlux);

    double* GetValuePointer(const wxString& name);

    bool ComputeAggregatedValues();

  protected:
    float m_area; // m2
    float m_elevation; // m.a.s.l.
    double m_outletTotal;
    bool m_needsCleanup;
    std::vector<HydroUnit*> m_hydroUnits;
    std::vector<Connector*> m_inConnectors;
    std::vector<Connector*> m_outConnectors;
    std::vector<Flux*> m_outletFluxes;
    std::vector<Behaviour*> m_behaviours;

  private:
};

#endif
