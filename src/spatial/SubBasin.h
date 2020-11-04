
#ifndef FLHY_SUBBASIN_H
#define FLHY_SUBBASIN_H

#include "Includes.h"
#include "Connector.h"
#include "HydroUnit.h"
#include "TimeMachine.h"

class SubBasin : public wxObject {
  public:
    SubBasin();

    ~SubBasin() override = default;

    bool IsOk();

    void AddHydroUnit(HydroUnit* unit);

    int GetHydroUnitsCount();

    bool HasIncomingFlow();

    void AddInputConnector(Connector* connector);

    void AddOutputConnector(Connector* connector);

    void AddContainer(Container* container);

    void AddFlux(Flux* flux);

  protected:
    float m_area; // m2
    std::vector<HydroUnit*> m_hydroUnits;
    std::vector<Connector*> m_inConnectors;
    std::vector<Connector*> m_outConnectors;
    std::vector<Container*> m_lumpedContainers;
    std::vector<Flux*> m_lumpedFluxes;

  private:
};

#endif
