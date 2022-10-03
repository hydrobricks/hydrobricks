#ifndef HYDROBRICKS_SUBBASIN_H
#define HYDROBRICKS_SUBBASIN_H

#include "Behaviour.h"
#include "Connector.h"
#include "HydroUnit.h"
#include "Includes.h"
#include "SettingsBasin.h"
#include "TimeMachine.h"

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

    vecInt GetHydroUnitsIds();

    int GetBricksCount();

    int GetSplittersCount();

    Brick* GetBrick(int index);

    bool HasBrick(const std::string& name);

    Brick* GetBrick(const std::string& name);

    Splitter* GetSplitter(int index);

    bool HasSplitter(const std::string& name);

    Splitter* GetSplitter(const std::string& name);

    bool HasIncomingFlow();

    void AddInputConnector(Connector* connector);

    void AddOutputConnector(Connector* connector);

    void AddBehaviour(Behaviour* behaviour);

    void AttachOutletFlux(Flux* pFlux);

    double* GetValuePointer(const std::string& name);

    bool ComputeOutletDischarge();

    float GetArea() {
        return m_area;
    }

  protected:
    float m_area;  // m2
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
