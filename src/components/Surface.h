#ifndef HYDROBRICKS_SURFACE_H
#define HYDROBRICKS_SURFACE_H

#include "Brick.h"
#include "Includes.h"

class Surface : public Brick {
  public:
    Surface(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::IsOk()
     */
    bool IsOk() override;

    /**
     * @copydoc Brick::Compute()
     */
    bool Compute() override;

  protected:
    double m_waterHeight;

  private:
};

#endif  // HYDROBRICKS_SURFACE_H
