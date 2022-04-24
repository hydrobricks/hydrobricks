#ifndef HYDROBRICKS_FLUX_CONNECTOR_FORCING_TO_BRICK_H
#define HYDROBRICKS_FLUX_CONNECTOR_FORCING_TO_BRICK_H

#include "FluxConnector.h"
#include "Forcing.h"
#include "Includes.h"

class Brick;

class FluxConnectorForcingToBrick : public FluxConnector {
  public:
    explicit FluxConnectorForcingToBrick();

  protected:
    Forcing* m_in;
    Brick* m_out;

  private:
};

#endif  // HYDROBRICKS_FLUX_CONNECTOR_FORCING_TO_BRICK_H
