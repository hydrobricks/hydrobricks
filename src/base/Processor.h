#ifndef HYDROBRICKS_PROCESSOR_H
#define HYDROBRICKS_PROCESSOR_H

#include "Includes.h"
#include "Solver.h"
#include "Brick.h"

class ModelHydro;

class Processor : public wxObject {
  public:
    explicit Processor();

    ~Processor();

    void Initialize(const SolverSettings &solverSettings);

    void SetModel(ModelHydro* model);

    void ConnectToElementsToSolve();

    int GetNbStateVariables();

    bool ProcessTimeStep();

    vecDoublePt* GetStateVariablesVectorPt() {
        return &m_stateVariables;
    }

    std::vector<Brick*>* GetIterableBricksVectorPt() {
        return &m_iterableBricks;
    }

    int GetNbConnections() {
        return m_connectionsNb;
    }
        
  protected:
    Solver* m_solver;
    ModelHydro* m_model;
    int m_connectionsNb;
    vecDoublePt m_stateVariables;
    std::vector<Brick*> m_iterableBricks;

  private:
    void StoreStateVariables(vecDoublePt& values);
};

#endif  // HYDROBRICKS_PROCESSOR_H
