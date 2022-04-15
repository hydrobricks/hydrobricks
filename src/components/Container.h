
#ifndef HYDROBRICKS_CONTAINER_H
#define HYDROBRICKS_CONTAINER_H

#include "Includes.h"
#include "Flux.h"

class HydroUnit;

class Container : public wxObject {
  public:
    explicit Container(HydroUnit* hydroUnit);

    ~Container() override = default;

    double GetWaterContent() {
        return m_waterContent;
    }

    bool HasLateralTransfer() {
        return !m_lateralOutputs.empty();
    }

    void TransferVertically(double &volume, double &rejected);

    void TransferHorizontally(double &volume, double &rejected);

  protected:
    double m_waterContent; // [mm]
    HydroUnit* m_hydroUnit;
    std::vector<Flux*> m_verticalOutputs;
    std::vector<Flux*> m_lateralOutputs;

  private:
};

#endif  // HYDROBRICKS_CONTAINER_H
