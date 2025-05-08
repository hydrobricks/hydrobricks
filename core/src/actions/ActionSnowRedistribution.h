#ifndef HYDROBRICKS_ACTION_SNOW_REDISTRIBUTION_H
#define HYDROBRICKS_ACTION_SNOW_REDISTRIBUTION_H

#include "Action.h"
#include "Includes.h"

class ActionSnowRedistribution : public Action {
  public:
    ActionSnowRedistribution();

    ~ActionSnowRedistribution() override = default;

    bool Apply(double date) override;

  protected:
  private:
};

#endif  // HYDROBRICKS_ACTION_SNOW_REDISTRIBUTION_H
