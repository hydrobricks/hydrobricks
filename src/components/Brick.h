#ifndef HYDROBRICKS_BRICK_H
#define HYDROBRICKS_BRICK_H

#include "Includes.h"
#include "Flux.h"

class HydroUnit;

class Brick : public wxObject {
  public:
    explicit Brick(HydroUnit* hydroUnit);

    ~Brick() override = default;

    double GetWaterContent() {
        return m_waterContent;
    }

  protected:
    double m_waterContent; // [mm]
    HydroUnit* m_hydroUnit;
    std::vector<Flux*> m_Inputs;
    std::vector<Flux*> m_Outputs;

  private:
};

#endif  // HYDROBRICKS_BRICK_H
