#ifndef HYDROBRICKS_SOLVER_EULER_EXPLICIT_H
#define HYDROBRICKS_SOLVER_EULER_EXPLICIT_H

#include "Includes.h"
#include "Solver.h"
#include "SubBasin.h"

class SolverEulerExplicit : public Solver {
  public:
    explicit SolverEulerExplicit();

    bool Solve() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_SOLVER_EULER_EXPLICIT_H
