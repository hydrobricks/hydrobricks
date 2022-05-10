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

    void SaveStateVariables(int col);

    void ComputeChangeRates(int col);

    void SetStateVariablesToIteration(int col);

    void SetStateVariablesToAvgOf(int col1, int col2);

    void ApplyProcesses(int col) const;

    void ApplyProcesses(const axd& changeRates) const;

  private:
};


#endif  // HYDROBRICKS_SOLVER_H
