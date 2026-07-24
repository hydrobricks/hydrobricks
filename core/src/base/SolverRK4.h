#ifndef HYDROBRICKS_SOLVER_RK4_H
#define HYDROBRICKS_SOLVER_RK4_H

#include "Includes.h"
#include "Solver.h"

class SolverRK4 : public Solver {
  public:
    explicit SolverRK4();

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
    axd _k3;
    axd _k4;
    axd _combinedRates;
    axd _state;
};

#endif  // HYDROBRICKS_SOLVER_RK4_H
