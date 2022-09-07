#ifndef HYDROBRICKS_SOLVER_H
#define HYDROBRICKS_SOLVER_H

#include "Includes.h"
#include "SettingsModel.h"

class Processor;

class Solver : public wxObject {
  public:
    explicit Solver();

    /**
     * Apply the solver.
     *
     * @return True if success, false otherwise.
     */
    virtual bool Solve() = 0;

    /**
     * Create the solver based on the provided settings.
     *
     * @param solverSettings The solver settings.
     * @return A new generated solver.
     */
    static Solver* Factory(const SolverSettings &solverSettings);

    /**
     * Connect the solver to a processor.
     *
     * @param processor The processor to connect with.
     */
    void Connect(Processor* processor) {
        wxASSERT(processor);
        m_processor = processor;
    }

    /**
     * Initialize the internal containers to the needed size.
     */
    void InitializeContainers();

  protected:
    Processor* m_processor;
    axxd m_stateVariableChanges;
    axxd m_changeRates;
    int m_nIterations;

    /**
     * Save the state variables.
     *
     * @param col The column of the internal storage where the state variables must be saved.
     */
    void SaveStateVariables(int col);

    /**
     * Compute the change rates of all processes.
     *
     * @param col The column of the internal storage where the change rates must be saved (= iteration).
     * @param applyConstraints Option to apply the constraints (e.g., storage max capacity).
     */
    void ComputeChangeRates(int col, bool applyConstraints = true);

    /**
     * Enforce the constraints for the change rates in the provided column.
     *
     * @param col The column (= iteration) of the internal storage containing the change rates.
     */
    void ApplyConstraintsFor(int col);

    /**
     * Reset the stored state variable changes to zero.
     */
    void ResetStateVariableChanges();

    /**
     * Set the state variables to use to the values of the selected iteration (column).
     * @param col The column (= iteration) of the internal storage containing the state variables to use.
     */
    void SetStateVariablesToIteration(int col);

    /**
     * Set the state variables to use to the average values of the selected iterations (columns).
     * @param col1 The first column (= iteration) of the internal storage containing the state variables to use.
     * @param col2 The second column (= iteration) of the internal storage containing the state variables to use.
     */
    void SetStateVariablesToAvgOf(int col1, int col2);

    /**
     * Apply the changes to the processes.
     *
     * @param col The column (= iteration) of the internal storage containing the change rates to use.
     */
    void ApplyProcesses(int col) const;

    /**
     * Apply the changes to the processes using the provided change rates.
     *
     * @param changeRates The change rate values to use.
     */
    void ApplyProcesses(const axd& changeRates) const;

    /**
     * Apply all changes.
     */
    void Finalize() const;

  private:
};


#endif  // HYDROBRICKS_SOLVER_H
