#ifndef HYDROBRICKS_PROCESSOR_H
#define HYDROBRICKS_PROCESSOR_H

#include "Includes.h"
#include "Solver.h"

class ModelHydro;

class Processor : public wxObject {
  public:
    explicit Processor(Solver* solver);

    void SetModel(ModelHydro* model);

    bool ProcessTimeStep();


  protected:
    Solver* m_solver;
    ModelHydro* m_model;

  private:
};

#endif  // HYDROBRICKS_PROCESSOR_H
