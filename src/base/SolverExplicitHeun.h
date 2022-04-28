#ifndef HYDROBRICKS_SOLVER_EXPLICIT_HEUN_H
#define HYDROBRICKS_SOLVER_EXPLICIT_HEUN_H

#include "Includes.h"
#include "Solver.h"
#include "SubBasin.h"

class SolverExplicitHeun : public Solver {
  public:
    explicit SolverExplicitHeun();

    bool Solve() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_SOLVER_EXPLICIT_HEUN_H
