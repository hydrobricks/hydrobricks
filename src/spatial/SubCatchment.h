
#ifndef FLHY_SUBCATCHMENT_H
#define FLHY_SUBCATCHMENT_H

#include "Includes.h"
#include "HydroUnit.h"
#include "Connector.h"

class SubCatchment : public wxObject {
  public:
    SubCatchment();

    ~SubCatchment() override = default;

    void AddHydroUnit(HydroUnit* unit);

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
