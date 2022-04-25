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
     * @copydoc Brick::NeedsSolver()
     */
    bool NeedsSolver() override;

    /**
     * @copydoc Brick::Compute()
     */
    bool Compute() override;

    /**
     * Sums the water amount from the different fluxes.
     *
     * @return sum of the water amount [mm]
     */
    double SumIncomingFluxes();

    void SetCapacity(double capacity) {
        m_capacity = capacity;
    }

    bool IsFull() {
        return m_contentPrev >= m_capacity;
    }

  protected:
    double m_capacity;

  private:
};

#endif  // HYDROBRICKS_STORAGE_H
