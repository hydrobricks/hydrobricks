#ifndef HYDROBRICKS_PROCESS_INFILTRATION_H
#define HYDROBRICKS_PROCESS_INFILTRATION_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessInfiltration : public Process {
  public:
    explicit ProcessInfiltration(WaterContainer* container);

    ~ProcessInfiltration() override = default;

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    bool NeedsTargetBrickLinking() override {
        return true;
    }

    int GetConnectionsNb() override;

    double* GetValuePointer(const string& name) override;

    void SetTargetBrick(Brick* targetBrick) override {
        m_targetBrick = targetBrick;
    }

  protected:
    Brick* m_targetBrick;

    double GetTargetStock();

    double GetTargetCapacity();

    double GetTargetFillingRatio();

  private:
};

#endif  // HYDROBRICKS_PROCESS_INFILTRATION_H
