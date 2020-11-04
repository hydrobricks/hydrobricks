
#ifndef FLHY_SUBBASIN_H
#define FLHY_SUBBASIN_H

#include "Includes.h"
#include "Connector.h"
#include "HydroUnit.h"
#include "Behaviour.h"
#include "TimeMachine.h"

class SubBasin : public wxObject {
  public:
    SubBasin();

    ~SubBasin() override;

    bool IsOk();

    void AddHydroUnit(HydroUnit* unit);

    int GetHydroUnitsCount();

    bool HasIncomingFlow();

    void AddInputConnector(Connector* connector);

    void AddOutputConnector(Connector* connector);

    void AddContainer(Container* container);

    void AddBehaviour(Behaviour* behaviour);

    void AddFlux(Flux* flux);

  protected:
    float m_area; // m2
    std::vector<HydroUnit*> m_hydroUnits;
    std::vector<Connector*> m_inConnectors;
    std::vector<Connector*> m_outConnectors;
    std::vector<Container*> m_lumpedContainers;
    std::vector<Flux*> m_lumpedFluxes;
    std::vector<Behaviour*> m_behaviours;

  private:
};

#endif
