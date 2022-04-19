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
     * @copydoc Brick::Compute()
     */
    bool Compute() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_SNOWPACK_H
