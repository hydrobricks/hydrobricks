#ifndef HYDROBRICKS_SOLVER_CRANK_NICOLSON_H
#define HYDROBRICKS_SOLVER_CRANK_NICOLSON_H

#include "Includes.h"
#include "SolverSequential.h"

/**
 * Sequential Crank-Nicolson (implicit trapezoidal) solver.
 *
 * Each brick is advanced by solving the implicit equation
 *
 *   S(t+h) = S(t) + h (I - (Q(S(t)) + Q(S(t+h))) / 2)
 *
 * where Q(S) is the total outflow rate of the brick's processes. The scalar
 * equation is solved by bisection, which is robust because Q is non-decreasing
 * in S. The applied rates are the average of the start- and end-of-step process
 * rates, making the scheme second-order accurate and unconditionally stable
 * (A-stable). For very stiff reservoirs (k h >> 1) it can produce a decaying
 * oscillation where implicit Euler stays monotone.
 */
class SolverCrankNicolson : public SolverSequential {
  public:
    explicit SolverCrankNicolson();

  protected:
    /**
     * @copydoc SolverSequential::ComputeBrickRates()
     */
    void ComputeBrickRates(Brick* brick, double content, double inflow, double timeStepInDays, int iRateStart) override;

  private:
    vecDouble _startRates;  // scratch: per-connection rates at the start-of-step content
    vecDouble _endRates;    // scratch: per-connection rates at the end-of-step content
};

#endif  // HYDROBRICKS_SOLVER_CRANK_NICOLSON_H
