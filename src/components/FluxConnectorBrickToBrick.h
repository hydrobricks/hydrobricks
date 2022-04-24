#ifndef HYDROBRICKS_FLUX_CONNECTOR_BRICK_TO_BRICK_H
#define HYDROBRICKS_FLUX_CONNECTOR_BRICK_TO_BRICK_H

#include "FluxConnector.h"
#include "Includes.h"

class Brick;

class FluxConnectorBrickToBrick : public FluxConnector {
  public:
    explicit FluxConnectorBrickToBrick();

  protected:
    Brick* m_in;
    Brick* m_out;

  private:
};

#endif  // HYDROBRICKS_FLUX_CONNECTOR_BRICK_TO_BRICK_H
