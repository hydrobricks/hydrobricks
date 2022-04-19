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
     * @copydoc Brick::Compute()
     */
    bool Compute() override;

    void SetCapacity(double capacity) {
        m_capacity = capacity;
    }

    bool IsFull() {
        return m_waterContent >= m_capacity;
    }

  protected:
    double m_capacity;

  private:
};

#endif  // HYDROBRICKS_STORAGE_H
