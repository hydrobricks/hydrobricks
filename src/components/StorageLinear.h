
#ifndef HYDROBRICKS_STORAGE_LINEAR_H
#define HYDROBRICKS_STORAGE_LINEAR_H

#include "Includes.h"
#include "HydroUnit.h"
#include "Storage.h"

class StorageLinear : public Storage {
  public:
    explicit StorageLinear(HydroUnit *hydroUnit);

    ~StorageLinear() override = default;

    double GetOutput();

    void SetResponseFactor(float *value) {
        m_responseFactor = value;
    }

    float GetResponseFactor() {
        return *m_responseFactor;
    }

  protected:
    float *m_responseFactor;  // [1/t]

  private:
};

#endif  // HYDROBRICKS_STORAGE_LINEAR_H
