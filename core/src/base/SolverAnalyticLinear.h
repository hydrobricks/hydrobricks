#ifndef HYDROBRICKS_SOLVER_ANALYTIC_LINEAR_H
#define HYDROBRICKS_SOLVER_ANALYTIC_LINEAR_H

#include "Includes.h"
#include "SolverSequential.h"

/**
 * Sequential solver with exact integration of linear reservoirs.
 *
 * Each brick is integrated exactly where its outflow is linear in the content
 * (dS/dt = I - q_frozen - k S); the rates of the other processes (e.g. ET) are
 * frozen at their start-of-step value. The resulting volumes are converted to
 * average rates over the step, so the standard constraint and flux machinery
 * applies unchanged.
 *
 * Compared to the staged explicit solvers, this scheme is unconditionally
 * stable, timestep-insensitive for linear stores, and propagates water through
 * a cascade within the same step (no response lag beyond the physical
 * residence time 1/k).
 */
class SolverAnalyticLinear : public SolverSequential {
  public:
    explicit SolverAnalyticLinear();

  protected:
    /**
     * @copydoc SolverSequential::ComputeBrickRates()
     */
    void ComputeBrickRates(Brick* brick, double content, double inflow, double timeStepInDays, int iRateStart) override;
};

#endif  // HYDROBRICKS_SOLVER_ANALYTIC_LINEAR_H
