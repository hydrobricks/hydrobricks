#ifndef HYDROBRICKS_SNOW_CONTAINER_H
#define HYDROBRICKS_SNOW_CONTAINER_H

#include "Includes.h"
#include "WaterContainer.h"

class Brick;

class SnowContainer : public WaterContainer {
  public:
    SnowContainer(Brick* brick);
};

#endif  // HYDROBRICKS_SNOW_CONTAINER_H
