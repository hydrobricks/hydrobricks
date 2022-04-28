#ifndef HYDROBRICKS_SOLVER_EXPLICIT_EULER_H
#define HYDROBRICKS_SOLVER_EXPLICIT_EULER_H

#include "Includes.h"
#include "Solver.h"
#include "SubBasin.h"

class SolverExplicitEuler : public Solver {
  public:
    explicit SolverExplicitEuler();

    bool Solve() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_SOLVER_EXPLICIT_EULER_H
