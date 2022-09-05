#ifndef HYDROBRICKS_SOLVER_HEUN_EXPLICIT_H
#define HYDROBRICKS_SOLVER_HEUN_EXPLICIT_H

#include "Includes.h"
#include "Solver.h"
#include "SubBasin.h"

class SolverHeunExplicit : public Solver {
  public:
    explicit SolverHeunExplicit();

    bool Solve() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_SOLVER_HEUN_EXPLICIT_H
