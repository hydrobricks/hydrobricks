#ifndef HYDROBRICKS_STORAGE_H
#define HYDROBRICKS_STORAGE_H

#include "Brick.h"
#include "Includes.h"

class Storage : public Brick {
  public:
    Storage(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::IsOk()
     */
    bool IsOk() override;

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

  protected:
    /**
     * @copydoc Brick::ComputeOutputs()
     */
    vecDouble ComputeOutputs() override;

  private:
};

#endif  // HYDROBRICKS_STORAGE_H
