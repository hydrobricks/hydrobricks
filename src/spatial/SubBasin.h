
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

    void AddHydroUnit(HydroUnit* unit);

    int GetHydroUnitsCount();

    bool HasIncomingFlow();

    void AddInputConnector(Connector* connector);

    void AddOutputConnector(Connector* connector);

  protected:
    std::vector<HydroUnit*> m_hydroUnits;
    std::vector<Connector*> m_inConnectors;
    std::vector<Connector*> m_outConnectors;

  private:
};

#endif
