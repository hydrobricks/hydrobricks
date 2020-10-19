
#ifndef FLHY_CONTAINER_H
#define FLHY_CONTAINER_H

#include "Includes.h"

class HydroUnit;

class Container : public wxObject {
  public:
    explicit Container(HydroUnit* hydroUnit);

    ~Container() override = default;

    double GetWaterContent() {
        return m_waterContent;
    }

    bool HasHorizontalTransfer() {
        return !m_horizontalOutputs.empty();
    }

    void TransferVertically(double &volume, double &rejected);

    void TransferHorizontally(double &volume, double &rejected);

  protected:
    double m_waterContent; // [mm]
    HydroUnit* m_hydroUnit;
    std::vector<Container*> m_verticalOutputs;
    std::vector<Container*> m_horizontalOutputs;

  private:
};

#endif  // FLHY_CONTAINER_H
