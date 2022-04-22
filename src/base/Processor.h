#ifndef HYDROBRICKS_PROCESSOR_H
#define HYDROBRICKS_PROCESSOR_H

#include "Includes.h"
#include "Solver.h"

class ModelHydro;

class Processor : public wxObject {
  public:
    explicit Processor(Solver* solver);

    void SetModel(ModelHydro* model);

    void ConnectToIterableElements();

    bool ProcessTimeStep();

  protected:
    Solver* m_solver;
    ModelHydro* m_model;
    std::vector<double*> m_iterableElements;

  private:
    void StoreIterableElements(std::vector<double*>& elements);
};

#endif  // HYDROBRICKS_PROCESSOR_H
