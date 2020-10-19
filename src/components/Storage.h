
#ifndef FLHY_STORAGE_H
#define FLHY_STORAGE_H

#include "Includes.h"
#include "Container.h"

class Storage : public Container {
  public:
    Storage(HydroUnit *hydroUnit);

    ~Storage() override = default;

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

#endif  // FLHY_STORAGE_H
