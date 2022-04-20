#ifndef HYDROBRICKS_PROCESS_H
#define HYDROBRICKS_PROCESS_H

#include "Includes.h"

class Process : public wxObject {
  public:
    Process();

    ~Process() override = default;

    virtual double GetWaterExtraction();

    virtual double GetWaterAddition();

  protected:
  private:
};

#endif  // HYDROBRICKS_PROCESS_H
