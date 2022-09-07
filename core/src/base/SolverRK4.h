#ifndef HYDROBRICKS_SOLVER_RK4_H
#define HYDROBRICKS_SOLVER_RK4_H

#include "Includes.h"
#include "Solver.h"
#include "SubBasin.h"

class SolverRK4 : public Solver {
  public:
    explicit SolverRK4();

    /**
     * @copydoc Solver::Solve()
     */
    bool Solve() override;

  protected:

  private:
};

#endif  // HYDROBRICKS_SOLVER_RK4_H
