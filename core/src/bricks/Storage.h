#ifndef HYDROBRICKS_STORAGE_H
#define HYDROBRICKS_STORAGE_H

#include "Brick.h"
#include "Includes.h"

class Storage : public Brick {
  public:
    Storage();

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

  protected:
  private:
};

#endif  // HYDROBRICKS_STORAGE_H
