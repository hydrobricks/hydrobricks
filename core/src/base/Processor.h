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
        return &m_stateVariableChanges;
    }

    /**
     * Get the iterable bricks vector pointer.
     *
     * @return the pointer to the iterable bricks vector.
     */
    vector<Brick*>* GetIterableBricksVectorPt() {
        return &m_iterableBricks;
    }

    /**
     * Get the number of solvable connections.
     *
     * @return the number of solvable connections.
     */
    int GetNbSolvableConnections() const {
        return m_solvableConnectionsNb;
    }

    /**
     * Get the number of direct connections.
     *
     * @return the number of direct connections.
     */
    int GetNbDirectConnections() const {
        return m_directConnectionsNb;
    }

  protected:
    Solver* m_solver;
    ModelHydro* m_model;
    int m_solvableConnectionsNb;
    int m_directConnectionsNb;
    vecDoublePt m_stateVariableChanges;
    vector<Brick*> m_iterableBricks;
    axd m_changeRatesNoSolver;

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
