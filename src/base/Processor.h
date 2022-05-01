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

    void ConnectToIterableBricks();

    void ConnectToIterableValues();

    int GetNbIterableValues();

    bool ProcessTimeStep();

    vecDoublePt* GetIterableValuesVectorPt() {
        return &m_iterableValues;
    }

    std::vector<Brick*>* GetIterableBricksVectorPt() {
        return &m_iterableBricks;
    }
        
  protected:
    Solver* m_solver;
    ModelHydro* m_model;
    vecDoublePt m_iterableValues;
    std::vector<Brick*> m_iterableBricks;

  private:
    void StoreIterableValues(vecDoublePt& values);
};

#endif  // HYDROBRICKS_PROCESSOR_H
