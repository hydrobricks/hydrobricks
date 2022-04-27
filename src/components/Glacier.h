#ifndef HYDROBRICKS_GLACIER_H
#define HYDROBRICKS_GLACIER_H

#include "Brick.h"
#include "Includes.h"

class Glacier : public Brick {
  public:
    Glacier(HydroUnit *hydroUnit);

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

#endif  // HYDROBRICKS_GLACIER_H
