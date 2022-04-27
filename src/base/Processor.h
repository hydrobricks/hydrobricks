#ifndef HYDROBRICKS_PROCESSOR_H
#define HYDROBRICKS_PROCESSOR_H

#include "Includes.h"
#include "Solver.h"
#include "Brick.h"

class ModelHydro;

class Processor : public wxObject {
  public:
    explicit Processor(Solver* solver);

    void SetModel(ModelHydro* model);

    void Initialize();

    void ConnectToIterableBricks();

    void ConnectToIterableValues();

    int GetNbIterableValues();

    bool ProcessTimeStep();

    std::vector<double*>* GetIterableValuesVectorPt() {
        return &m_iterableValues;
    }

    std::vector<Brick*>* GetIterableBricksVectorPt() {
        return &m_iterableBricks;
    }
        
  protected:
    Solver* m_solver;
    ModelHydro* m_model;
    std::vector<double*> m_iterableValues;
    std::vector<Brick*> m_iterableBricks;

  private:
    void StoreIterableValues(std::vector<double*>& values);
};

#endif  // HYDROBRICKS_PROCESSOR_H
