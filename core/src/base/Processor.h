#ifndef HYDROBRICKS_PROCESSOR_H
#define HYDROBRICKS_PROCESSOR_H

#include <memory>

#include "Brick.h"
#include "Includes.h"
#include "Solver.h"

class ModelHydro;

class Processor {
  public:
    explicit Processor();

    virtual ~Processor();

    /**
     * Initialize the processor with the solver settings.
     *
     * @param solverSettings settings of the solver.
     */
    void Initialize(const SolverSettings& solverSettings);

    /**
     * Set the model.
     *
     * @param model model to set.
     */
    void SetModel(ModelHydro* model);

    /**
     * Connect the processor to the elements to solve.
     */
    void ConnectToElementsToSolve();

    /**
     * Get the number of state variables.
     *
     * @return the number of state variables.
     */
    int GetStateVariableCount() const;

    /**
     * Process the time step.
     *
     * @param timeStepInDays The time step in days.
     * @return true if the time step was processed successfully, false otherwise.
     */
    bool ProcessTimeStep(double timeStepInDays);

    /**
     * Get the state variables vector pointer.
     *
     * @return the pointer to the state variables vector.
     */
    vecDoublePt* GetStateVariablesVectorPt() {
        return &_stateVariableChanges;
    }

    /**
     * Get the iterable bricks vector pointer.
     *
     * @return the pointer to the iterable bricks vector.
     */
    vector<Brick*>* GetIterableBricksVectorPt() {
        return &_iterableBricks;
    }

    /**
     * Get the number of solvable connections.
     *
     * @return the number of solvable connections.
     */
    int GetSolvableConnectionCount() const {
        return _solvableConnectionCount;
    }

    /**
     * Get the number of direct connections.
     *
     * @return the number of direct connections.
     */
    int GetDirectConnectionCount() const {
        return _directConnectionCount;
    }

  protected:
    std::unique_ptr<Solver> _solver;  // owning
    ModelHydro* _model;               // non-owning reference
    int _solvableConnectionCount;
    int _directConnectionCount;
    vecDoublePt _stateVariableChanges;
    vector<Brick*> _iterableBricks;  // non-owning views into HydroUnits/SubBasin
    axd _changeRatesNoSolver;

  private:
    /**
     * Store the state variable changes.
     *
     * @param values the state variable changes to store.
     */
    void StoreStateVariableChanges(vecDoublePt& values);

    /**
     * Apply direct changes to the brick.
     *
     * @param brick the brick to apply changes to.
     * @param ptIndex the index of the point in the state variable vector.
     * @param timeStepInDays the time step in days.
     */
    void ApplyDirectChanges(Brick* brick, int& ptIndex, double timeStepInDays);
};

#endif  // HYDROBRICKS_PROCESSOR_H
