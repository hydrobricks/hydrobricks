#ifndef HYDROBRICKS_SOLVER_H
#define HYDROBRICKS_SOLVER_H

#include <memory>

#include "Includes.h"
#include "SettingsModel.h"

class Processor;

class Solver {
  public:
    explicit Solver();

    virtual ~Solver() = default;

    /**
     * Apply the solver.
     *
     * @param timeStepInDays The time step in days.
     * @return True if success, false otherwise.
     */
    virtual bool Solve(double timeStepInDays) = 0;

    /**
     * Create the solver based on the provided settings.
     *
     * @param solverSettings The solver settings.
     * @return A new generated solver.
     */
    static std::unique_ptr<Solver> Factory(const SolverSettings& solverSettings);

    /**
     * Connect the solver to a processor.
     *
     * @param processor The processor to connect with.
     */
    void Connect(Processor* processor) {
        assert(processor);
        _processor = processor;
    }

    /**
     * Allocate the internal rate and state buffers to the needed size.
     * Must be called after Connect(), once the processor knows its connection and state counts.
     */
    virtual void InitializeContainers() = 0;

  protected:
    Processor* _processor;
};

#endif  // HYDROBRICKS_SOLVER_H
