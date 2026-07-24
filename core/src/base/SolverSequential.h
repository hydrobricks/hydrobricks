#ifndef HYDROBRICKS_SOLVER_SEQUENTIAL_H
#define HYDROBRICKS_SOLVER_SEQUENTIAL_H

#include "Includes.h"
#include "Solver.h"

class Brick;

/**
 * Base class for the sequential solvers.
 *
 * The bricks are processed one by one in declaration order (upstream before
 * downstream). Each brick receives its inflows as a constant rate over the time
 * step; the subclass computes the brick's average outflow rates over the step.
 * The resulting rates go through the standard constraint and flux machinery, so
 * water propagates through a cascade within the same step.
 */
class SolverSequential : public Solver {
  public:
    /**
     * @copydoc Solver::InitializeContainers()
     */
    void InitializeContainers() override;

    /**
     * @copydoc Solver::Solve()
     */
    bool Solve(double timeStepInDays) final;

  protected:
    axd _rates;

    /**
     * Compute the average outflow rates of the brick over the time step and store
     * them in the _rates slice starting at iRateStart (one value per process
     * connection, in process declaration order).
     *
     * @param brick The brick to solve.
     * @param content Start-of-step content [mm], including instantaneous deposits.
     * @param inflow Inflows as a constant rate over the step [mm/d].
     * @param timeStepInDays The time step in days.
     * @param iRateStart Index of the brick's first connection in _rates.
     */
    virtual void ComputeBrickRates(Brick* brick, double content, double inflow, double timeStepInDays,
                                   int iRateStart) = 0;
};

#endif  // HYDROBRICKS_SOLVER_SEQUENTIAL_H
