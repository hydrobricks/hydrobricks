#ifndef HYDROBRICKS_SOLVER_H
#define HYDROBRICKS_SOLVER_H

#include "Includes.h"
#include "SettingsModel.h"

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

    void SetTimeStepInDays(double* timeStep) {
        m_timeStepInDays = timeStep;
    }

  protected:
    Processor* m_processor;
    axxd m_stateVariables;
    axxd m_changeRates;
    int m_nIterations;
    double* m_timeStepInDays;

  private:
};


#endif  // HYDROBRICKS_SOLVER_H
