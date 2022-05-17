#ifndef HYDROBRICKS_GLACIER_H
#define HYDROBRICKS_GLACIER_H

#include "Brick.h"
#include "Includes.h"

class Glacier : public Brick {
  public:
    Glacier(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

    void ApplyConstraints(double timeStep) override;

    void Finalize() override;

  protected:
    bool m_unlimitedSupply;

  private:
};

#endif  // HYDROBRICKS_GLACIER_H
