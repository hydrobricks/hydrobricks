#ifndef HYDROBRICKS_SOLVER_EULER_EXPLICIT_H
#define HYDROBRICKS_SOLVER_EULER_EXPLICIT_H

#include "Includes.h"
#include "Solver.h"

class SolverEulerExplicit : public Solver {
  public:
    explicit SolverEulerExplicit();

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
};

#endif  // HYDROBRICKS_SOLVER_EULER_EXPLICIT_H
