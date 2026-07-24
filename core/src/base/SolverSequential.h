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

    /**
     * Sum the rates of all processes of the brick, evaluated at the given content
     * offset. Temporarily writes the offset into the container's dynamic change;
     * the caller must restore it to zero before the constraint pass.
     *
     * @param brick The brick to evaluate.
     * @param contentDelta Pointer to the container's dynamic content change.
     * @param offset Content offset from the start-of-step content [mm].
     * @return the total outflow rate [mm/d].
     */
    static double TotalRateAt(Brick* brick, double* contentDelta, double offset);

    /**
     * Store the per-connection rates of the brick's processes, evaluated at the
     * current content, into the provided vector (resized to the connection count).
     *
     * @param brick The brick to evaluate.
     * @param rates The vector receiving the rates.
     */
    static void StoreRatesAtCurrentContent(Brick* brick, vecDouble& rates);
};

#endif  // HYDROBRICKS_SOLVER_SEQUENTIAL_H
