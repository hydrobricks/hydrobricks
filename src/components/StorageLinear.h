
#ifndef FLHY_STORAGE_LINEAR_H
#define FLHY_STORAGE_LINEAR_H

#include "Includes.h"
#include "HydroUnit.h"
#include "Storage.h"

class StorageLinear : public Storage {
  public:
    StorageLinear(HydroUnit *hydroUnit);

    ~StorageLinear() override = default;

    double GetOutput();

    void SetResponseFactor(double value) {
        m_responseFactor = value;
    }

  protected:
    double m_responseFactor; // [1/t]

  private:
};

#endif  // FLHY_STORAGE_LINEAR_H
