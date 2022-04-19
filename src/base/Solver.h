#ifndef HYDROBRICKS_SOLVER_H
#define HYDROBRICKS_SOLVER_H

#include "Includes.h"

class Solver : public wxObject {
  public:
    explicit Solver();

    virtual bool Solve() = 0;

  protected:

  private:
};


#endif  // HYDROBRICKS_SOLVER_H
