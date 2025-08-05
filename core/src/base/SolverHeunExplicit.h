#ifndef HYDROBRICKS_SOLVER_HEUN_EXPLICIT_H
#define HYDROBRICKS_SOLVER_HEUN_EXPLICIT_H

#include "Includes.h"
#include "Solver.h"
#include "SubBasin.h"

class SolverHeunExplicit : public Solver {
  public:
    explicit SolverHeunExplicit();

    /**
     * @copydoc Solver::Solve()
     */
    bool Solve() override;
};

#endif  // HYDROBRICKS_SOLVER_HEUN_EXPLICIT_H
