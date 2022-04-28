#ifndef HYDROBRICKS_SOLVER_H
#define HYDROBRICKS_SOLVER_H

#include "Includes.h"
#include "ParameterSet.h"

class Processor;

class Solver : public wxObject {
  public:
    explicit Solver();

    virtual bool Solve() = 0;

    static Solver* Factory(const SolverSettings &solverSettings);

    void Connect(Processor* processor) {
        wxASSERT(processor);
        m_processor = processor;
    }

    void InitializeContainers();

  protected:
    Processor* m_processor;
    Eigen::ArrayXXd m_container;
    int m_nIterations;

  private:
};


#endif  // HYDROBRICKS_SOLVER_H
