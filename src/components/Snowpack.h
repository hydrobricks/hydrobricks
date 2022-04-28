#ifndef HYDROBRICKS_SNOWPACK_H
#define HYDROBRICKS_SNOWPACK_H

#include "Brick.h"
#include "Includes.h"

class Snowpack : public Brick {
  public:
    Snowpack(HydroUnit *hydroUnit);

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
    std::vector<double> ComputeOutputs() override;

  private:
};

#endif  // HYDROBRICKS_SNOWPACK_H
