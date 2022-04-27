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

  protected:
    /**
     * @copydoc Brick::ComputeOutputs()
     */
    std::vector<double> ComputeOutputs() override;

  private:
};

#endif  // HYDROBRICKS_STORAGE_H
