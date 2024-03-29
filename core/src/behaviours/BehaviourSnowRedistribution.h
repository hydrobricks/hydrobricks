#ifndef HYDROBRICKS_BEHAVIOUR_SNOW_REDISTRIBUTION_H
#define HYDROBRICKS_BEHAVIOUR_SNOW_REDISTRIBUTION_H

#include "Behaviour.h"
#include "Includes.h"

class BehaviourSnowRedistribution : public Behaviour {
  public:
    BehaviourSnowRedistribution();

    ~BehaviourSnowRedistribution() override = default;

    bool Apply(double date) override;

  protected:
  private:
};

#endif  // HYDROBRICKS_BEHAVIOUR_SNOW_REDISTRIBUTION_H
