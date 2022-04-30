#ifndef HYDROBRICKS_PROCESS_H
#define HYDROBRICKS_PROCESS_H

#include "Includes.h"

class Brick;

class Process : public wxObject {
  public:
    Process(Brick* brick);

    ~Process() override = default;

    virtual double GetWaterExtraction();

    virtual double GetWaterAddition();

    /**
     * Get pointers to the values that need to be iterated.
     *
     * @return vector of pointers to the values that need to be iterated.
     */
    virtual vecDoublePt GetIterableValues() {
        return vecDoublePt {};
    }

  protected:
    Brick* m_brick;

  private:
};

#endif  // HYDROBRICKS_PROCESS_H
