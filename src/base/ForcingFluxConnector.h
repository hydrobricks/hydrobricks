#ifndef HYDROBRICKS_FORCING_FLUX_CONNECTOR_H
#define HYDROBRICKS_FORCING_FLUX_CONNECTOR_H

#include "Includes.h"

class Forcing;
class Flux;

class ForcingFluxConnector : public wxObject {
  public:
    ForcingFluxConnector(Forcing* forcing, Flux* flux);

    ~ForcingFluxConnector() override = default;

  protected:
    Forcing* m_forcing;
    Flux* m_flux;

  private:
};

#endif  // HYDROBRICKS_FORCING_FLUX_CONNECTOR_H
