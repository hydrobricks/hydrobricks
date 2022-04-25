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

    /**
     * @copydoc Brick::NeedsSolver()
     */
    bool NeedsSolver() override;

    /**
     * @copydoc Brick::Compute()
     */
    bool Compute() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_GLACIER_H
