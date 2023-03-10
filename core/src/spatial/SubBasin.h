#ifndef HYDROBRICKS_SUBBASIN_H
#define HYDROBRICKS_SUBBASIN_H

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

    void Reset();

    void SaveAsInitialState();

    bool IsOk();

    void AddBrick(Brick* brick);

    void AddSplitter(Splitter* splitter);

    void AddHydroUnit(HydroUnit* unit);

    int GetHydroUnitsNb();

    HydroUnit* GetHydroUnit(int index);

    HydroUnit* GetHydroUnitById(int id);

    vecInt GetHydroUnitIds();

    vecDouble GetHydroUnitAreas();

    int GetBricksCount();

    int GetSplittersCount();

    Brick* GetBrick(int index);

    bool HasBrick(const string& name);

    Brick* GetBrick(const string& name);

    Splitter* GetSplitter(int index);

    bool HasSplitter(const string& name);

    Splitter* GetSplitter(const string& name);

    bool HasIncomingFlow();

    void AddInputConnector(Connector* connector);

    void AddOutputConnector(Connector* connector);

    void AttachOutletFlux(Flux* pFlux);

    double* GetValuePointer(const string& name);

    bool ComputeOutletDischarge();

    double GetArea() {
        return m_area;
    }

  protected:
    double m_area;  // m2
    double m_outletTotal;
    bool m_needsCleanup;
    vector<Brick*> m_bricks;
    vector<Splitter*> m_splitters;
    vector<HydroUnit*> m_hydroUnits;
    vector<Connector*> m_inConnectors;
    vector<Connector*> m_outConnectors;
    vector<Flux*> m_outletFluxes;

  private:
};

#endif
