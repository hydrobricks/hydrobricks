
#ifndef HYDROBRICKS_STORAGE_LINEAR_H
#define HYDROBRICKS_STORAGE_LINEAR_H

#include "Includes.h"
#include "HydroUnit.h"
#include "Storage.h"

class StorageLinear : public Storage {
  public:
    /**
     * Create a linear storage
     *
     * @param hydroUnit spatial unit to attach the storage
     */
    explicit StorageLinear(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::IsOk()
     */
    bool IsOk() override;

    /**
     * Set the response factor value
     *
     * @param value response factor value [1/T]
     */
    void SetResponseFactor(float *value) {
        m_responseFactor = value;
    }

    /**
     * Get the response factor value
     *
     * @return response factor value [1/T]
     */
    float GetResponseFactor() {
        return *m_responseFactor;
    }

  protected:
    float *m_responseFactor;  // [1/T]

    /**
     * @copydoc Brick::ComputeOutputs()
     */
    std::vector<double> ComputeOutputs() override;

  private:
    double getOutputsSum(std::vector<double> &qOuts);
};

#endif  // HYDROBRICKS_STORAGE_LINEAR_H
