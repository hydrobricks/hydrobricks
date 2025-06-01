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

    /**
     * @copydoc Process::NeedsTargetBrickLinking()
     */
    bool NeedsTargetBrickLinking() override {
        return true;
    }

    /**
     * @copydoc Process::GetConnectionsNb()
     */
    int GetConnectionsNb() override;

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(const string& name) override;

    /**
     * @copydoc Process::SetTargetBrick()
     */
    void SetTargetBrick(Brick* targetBrick) override {
        m_targetBrick = targetBrick;
    }

  protected:
    Brick* m_targetBrick;

    /**
     * Get the water content of the target brick.
     *
     * @return The water content of the target brick.
     */
    double GetTargetStock();

    /**
     * Get the maximum capacity of the target brick.
     *
     * @return The maximum capacity of the target brick.
     */
    double GetTargetCapacity();

    /**
     * Get the filling ratio of the target brick.
     *
     * @return The filling ratio of the target brick.
     */
    double GetTargetFillingRatio();
};

#endif  // HYDROBRICKS_PROCESS_INFILTRATION_H
