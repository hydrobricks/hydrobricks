#ifndef HYDROBRICKS_SOLVER_HEUN_EXPLICIT_H
#define HYDROBRICKS_SOLVER_HEUN_EXPLICIT_H

#include "Includes.h"
#include "Solver.h"

class SolverHeunExplicit : public Solver {
  public:
    explicit SolverHeunExplicit();

    /**
     * @copydoc Solver::InitializeContainers()
     */
    void InitializeContainers() override;

    /**
     * @copydoc Solver::Solve()
     */
    bool Solve(double timeStepInDays) override;

  private:
    axd _k1;
    axd _k2;
    axd _combinedRates;
};

#endif  // HYDROBRICKS_SOLVER_HEUN_EXPLICIT_H
