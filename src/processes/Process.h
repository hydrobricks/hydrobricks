#ifndef HYDROBRICKS_PROCESS_H
#define HYDROBRICKS_PROCESS_H

#include "Includes.h"

class Process : public wxObject {
  public:
    Process();

    ~Process() override = default;

    virtual double GetWaterExtraction();

    virtual double GetWaterAddition();

    /**
     * Get pointers to the values that need to be iterated.
     *
     * @return vector of pointers to the values that need to be iterated.
     */
    virtual std::vector<double*> GetIterableElements() {
        return std::vector<double*> {};
    }

  protected:
  private:
};

#endif  // HYDROBRICKS_PROCESS_H
