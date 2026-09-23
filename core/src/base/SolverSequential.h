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
     * Store the per-connection rates of the brick's processes, evaluated at the given
     * content offset, and return their sum. The rates are computed once and used both
     * for the total and for the per-connection values, which matters because this sits
     * on the innermost solver loop.
     *
     * @param brick The brick to evaluate.
     * @param contentDelta Pointer to the container's dynamic content change.
     * @param offset Content offset from the start-of-step content [mm].
     * @param rates The vector receiving the rates.
     * @return the total outflow rate [mm/d].
     */
    static double StoreRatesAndTotalAt(Brick* brick, double* contentDelta, double offset, vecDouble& rates);

    /**
     * Sum the affine response coefficients of the brick's processes, evaluated at the
     * current content: every process must report an affine response (rate = k S - offset)
     * on a single connection for the sum to be meaningful.
     *
     * A process may be affine only over part of the content range (a threshold outflow is
     * simply off below its threshold, an empty store produces no outflow at all), so a
     * solution built from these coefficients is a candidate that still has to be checked
     * against the real process rates.
     *
     * @param brick The brick to inspect.
     * @param rate Receives the summed linear coefficient k [1/d].
     * @param offset Receives the summed offset [mm/d].
     * @return true if every process of the brick reports an affine response.
     */
    static bool SumAffineResponse(Brick* brick, double& rate, double& offset);
};

#endif  // HYDROBRICKS_SOLVER_SEQUENTIAL_H
