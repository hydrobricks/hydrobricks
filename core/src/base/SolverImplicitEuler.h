#ifndef HYDROBRICKS_SOLVER_IMPLICIT_EULER_H
#define HYDROBRICKS_SOLVER_IMPLICIT_EULER_H

#include "Includes.h"
#include "SolverSequential.h"

/**
 * Sequential implicit (backward) Euler solver.
 *
 * Each brick is advanced by solving the implicit equation
 *
 *   S(t+h) = S(t) + h (I - Q(S(t+h)))
 *
 * where Q(S) is the total outflow rate of the brick's processes evaluated at
 * the end-of-step content. The scalar equation is solved by bisection, which is
 * robust because Q is non-decreasing in S. All processes (including non-linear
 * ones such as ET) are evaluated at the end-of-step state, making the scheme
 * unconditionally stable: fast-reacting or strongly non-linear reservoirs
 * cannot destabilize it, at first-order accuracy.
 */
class SolverImplicitEuler : public SolverSequential {
  public:
    explicit SolverImplicitEuler();

  protected:
    /**
     * @copydoc SolverSequential::ComputeBrickRates()
     */
    void ComputeBrickRates(Brick* brick, double content, double inflow, double timeStepInDays, int iRateStart) override;

  private:
    /**
     * Sum the rates of all processes of the brick, evaluated at the given content.
     * Temporarily writes the content offset into the container's dynamic change.
     *
     * @param brick The brick to evaluate.
     * @param contentDelta Pointer to the container's dynamic content change.
     * @param offset Content offset from the start-of-step content [mm].
     * @return the total outflow rate [mm/d].
     */
    static double TotalRateAt(Brick* brick, double* contentDelta, double offset);
};

#endif  // HYDROBRICKS_SOLVER_IMPLICIT_EULER_H
