#ifndef HYDROBRICKS_SOLVER_EXPONENTIAL_EULER_H
#define HYDROBRICKS_SOLVER_EXPONENTIAL_EULER_H

#include "Includes.h"
#include "SolverSequential.h"

/**
 * Sequential exponential Euler solver (analytic integration of the linearized system).
 *
 * Each brick's total outflow is linearized around the start-of-step content,
 * Q(S) ~ Q(S0) + k (S - S0) with k obtained by finite difference, and the
 * linearized equation dS/dt = I - Q(S0) - k (S - S0) is integrated exactly over
 * the step. The scheme is exact for linear reservoirs (where the linearization
 * is the reservoir itself), second-order accurate for smooth non-linear
 * processes, and unconditionally stable. The total outflow volume is
 * distributed over the connections proportionally to their trapezoidal-average
 * rates.
 */
class SolverExponentialEuler : public SolverSequential {
  public:
    explicit SolverExponentialEuler();

  protected:
    /**
     * @copydoc SolverSequential::ComputeBrickRates()
     */
    void ComputeBrickRates(Brick* brick, double content, double inflow, double timeStepInDays, int iRateStart) override;

  private:
    vecDouble _startRates;  // scratch: per-connection rates at the start-of-step content
    vecDouble _endRates;    // scratch: per-connection rates at the end-of-step content
};

#endif  // HYDROBRICKS_SOLVER_EXPONENTIAL_EULER_H
