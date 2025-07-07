#ifndef HYDROBRICKS_PROCESSOR_H
#define HYDROBRICKS_PROCESSOR_H

#include "Brick.h"
#include "Includes.h"
#include "Solver.h"

class ModelHydro;

class Processor : public wxObject {
  public:
    explicit Processor();

    ~Processor() override;

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
    int GetNbStateVariables();

    /**
     * Process the time step.
     *
     * @return true if the time step was processed successfully, false otherwise.
     */
    bool ProcessTimeStep();

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
    int GetNbSolvableConnections() const {
        return _solvableConnectionsNb;
    }

    /**
     * Get the number of direct connections.
     *
     * @return the number of direct connections.
     */
    int GetNbDirectConnections() const {
        return _directConnectionsNb;
    }

  protected:
    Solver* _solver;
    ModelHydro* _model;
    int _solvableConnectionsNb;
    int _directConnectionsNb;
    vecDoublePt _stateVariableChanges;
    vector<Brick*> _iterableBricks;
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
     */
    void ApplyDirectChanges(Brick* brick, int& ptIndex);
};

#endif  // HYDROBRICKS_PROCESSOR_H
