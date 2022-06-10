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

    void AddBrick(Brick* brick);

    void AddSplitter(Splitter* splitter);

    void AddHydroUnit(HydroUnit* unit);

    int GetHydroUnitsNb();

    HydroUnit* GetHydroUnit(int index);

    int GetBricksCount();

    int GetSplittersCount();

    Brick* GetBrick(int index);

    bool HasBrick(const wxString &name);

    Brick* GetBrick(const wxString &name);

    Splitter* GetSplitter(int index);

    bool HasSplitter(const wxString &name);

    Splitter* GetSplitter(const wxString &name);

    bool HasIncomingFlow();

    void AddInputConnector(Connector* connector);

    void AddOutputConnector(Connector* connector);

    void AddBehaviour(Behaviour* behaviour);

    void AttachOutletFlux(Flux* pFlux);

    double* GetValuePointer(const wxString& name);

    bool ComputeOutletDischarge();

  protected:
    float m_area; // m2
    double m_outletTotal;
    bool m_needsCleanup;
    std::vector<Brick*> m_bricks;
    std::vector<Splitter*> m_splitters;
    std::vector<HydroUnit*> m_hydroUnits;
    std::vector<Connector*> m_inConnectors;
    std::vector<Connector*> m_outConnectors;
    std::vector<Flux*> m_outletFluxes;
    std::vector<Behaviour*> m_behaviours;

  private:
};

#endif
