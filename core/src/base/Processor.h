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

    void Initialize(const SolverSettings& solverSettings);

    void SetModel(ModelHydro* model);

    void ConnectToElementsToSolve();

    int GetNbStateVariables();

    bool ProcessTimeStep();

    vecDoublePt* GetStateVariablesVectorPt() {
        return &m_stateVariableChanges;
    }

    std::vector<Brick*>* GetIterableBricksVectorPt() {
        return &m_iterableBricks;
    }

    int GetNbSolvableConnections() const {
        return m_solvableConnectionsNb;
    }

    int GetNbDirectConnections() const {
        return m_directConnectionsNb;
    }

  protected:
    Solver* m_solver;
    ModelHydro* m_model;
    int m_solvableConnectionsNb;
    int m_directConnectionsNb;
    vecDoublePt m_stateVariableChanges;
    std::vector<Brick*> m_iterableBricks;
    axd m_changeRatesNoSolver;

  private:
    void StoreStateVariableChanges(vecDoublePt& values);

    void ApplyDirectChanges(Brick* brick, int& ptIndex);
};

#endif  // HYDROBRICKS_PROCESSOR_H
