#ifndef HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H
#define HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessOutflowLinear : public Process {
  public:
    explicit ProcessOutflowLinear(Brick* brick);

    ~ProcessOutflowLinear() override = default;

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    /**
     * @copydoc Process::AssignParameters()
     */
    void AssignParameters(const ProcessSettings &processSettings) override;

    double GetWaterExtraction() override;

    double* GetValuePointer(const wxString& name);

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
    float* m_responseFactor;  // [1/T]

  private:
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_LINEAR_H
