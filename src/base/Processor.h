#ifndef HYDROBRICKS_PROCESSOR_H
#define HYDROBRICKS_PROCESSOR_H

#include "Includes.h"
#include "Solver.h"

class ModelHydro;

class Processor : public wxObject {
  public:
    explicit Processor(Solver* solver);

    void SetModel(ModelHydro* model);

    void ConnectToIterableValues();

    bool ProcessTimeStep();

  protected:
    Solver* m_solver;
    ModelHydro* m_model;
    std::vector<double*> m_iterableValues;

  private:
    void StoreIterableValues(std::vector<double*>& values);
};

#endif  // HYDROBRICKS_PROCESSOR_H
