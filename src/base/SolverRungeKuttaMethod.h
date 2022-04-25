#ifndef HYDROBRICKS_SOLVER_RK4_H
#define HYDROBRICKS_SOLVER_RK4_H

#include "Includes.h"
#include "Solver.h"
#include "SubBasin.h"

class SolverRungeKuttaMethod : public Solver {
  public:
    explicit SolverRungeKuttaMethod();

    bool Solve(SubBasin* basin) override;

  protected:

  private:
};

#endif  // HYDROBRICKS_SOLVER_RK4_H
