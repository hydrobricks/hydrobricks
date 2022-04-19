#ifndef HYDROBRICKS_PROCESSOR_H
#define HYDROBRICKS_PROCESSOR_H

#include "Includes.h"
#include "Solver.h"

class Processor : public wxObject {
  public:
    explicit Processor(Solver* solver);

    bool Process();


  protected:
    Solver* m_solver;

  private:
};

#endif  // HYDROBRICKS_PROCESSOR_H
