#ifndef HYDROBRICKS_PROCESSOR_H
#define HYDROBRICKS_PROCESSOR_H

#include <memory>

#include "Brick.h"
#include "Includes.h"
#include "Solver.h"

class ModelHydro;
class Splitter;

class Processor {
  public:
    explicit Processor();

    virtual ~Processor();

    /**
     * Initialize the processor with the solver settings.
     *
     * @param solverSettings settings of the solver.
     */
    void Initialize(const SolverSettings& solverSettings);

    /**
     * Set the model.
     *
     * @param model model to set.
     */
    void SetModel(ModelHydro* model);

    /**
     * Connect the processor to the elements to solve.
     */
    void ConnectToElementsToSolve();

    /**
     * Get the number of state variables.
     *
     * @return the number of state variables.
     */
    [[nodiscard]] int GetStateVariableCount() const;

    /**
     * Process the time step.
     *
     * @param timeStepInDays The time step in days.
     * @return true if the time step was processed successfully, false otherwise.
     */
    bool ProcessTimeStep(double timeStepInDays);

    /**
     * Gather the current state variable values into the provided vector.
     *
     * The state variables are the dynamic content changes relative to the start of the
     * time step (all zeros at step start, after the previous Finalize).
     *
     * @param state The vector to fill (sized to GetStateVariableCount()).
     */
    void GatherState(axd& state) const;

    /**
     * Scatter the provided values into the state variables.
     *
     * @param state The state variable values to set (sized to GetStateVariableCount()).
     */
    void ScatterState(const axd& state);

    /**
     * Reset all state variables to zero (the start-of-step state).
     */
    void ResetState();

    /**
     * Evaluate the change rates of all solvable processes at the current state.
     *
     * Rates are per day, independent of the time step. When applyConstraints is true,
     * the rates are linked to the outgoing fluxes and the storage constraints (e.g.,
     * maximum capacity, no negative content) are enforced brick by brick during the sweep.
     *
     * @param rates The vector receiving the rates (sized to GetSolvableConnectionCount());
     *              must outlive any subsequent constraint application, as fluxes keep
     *              pointers into it.
     * @param timeStepInDays The time step in days (used by the constraints).
     * @param applyConstraints Option to apply the constraints.
     */
    void EvaluateRates(axd& rates, double timeStepInDays, bool applyConstraints = true);

    /**
     * Link the provided rates to the outgoing fluxes and enforce the storage constraints,
     * evaluated at the current state.
     *
     * @param rates The rate values to constrain (modified in place through the flux links).
     * @param timeStepInDays The time step in days.
     */
    void ConstrainRates(axd& rates, double timeStepInDays);

    /**
     * Sweep the brick storage constraints over the linked rates until they are stable
     * (fixpoint iteration), so that the result does not depend on the brick iteration
     * order. The fluxes must already be linked to the provided rates.
     *
     * @param rates The rate values to constrain (modified in place through the flux links).
     * @param timeStepInDays The time step in days.
     */
    void EnforceConstraints(axd& rates, double timeStepInDays);

    /**
     * Apply the provided change rates: transfer the water between the bricks and
     * accumulate the resulting content changes over the time step.
     *
     * @param rates The change rate values to apply.
     * @param timeStepInDays The time step in days.
     */
    void ApplyRates(const axd& rates, double timeStepInDays);

    /**
     * Finalize the solvable bricks: commit the accumulated content changes.
     */
    void FinalizeTimeStep();

    /**
     * Get the number of solvable connections.
     *
     * @return the number of solvable connections.
     */
    int GetSolvableConnectionCount() const {
        return _solvableConnectionCount;
    }

    /**
     * Get the bricks handled by the solver, in processing (declaration) order.
     *
     * @return the solvable bricks.
     */
    const vector<Brick*>& GetSolvableBricks() const {
        return _iterableBricks;
    }

    /**
     * Get the number of direct connections.
     *
     * @return the number of direct connections.
     */
    int GetDirectConnectionCount() const {
        return _directConnectionCount;
    }

    /**
     * Flat view of one solvable process: its rate slice in the global rates vector.
     */
    struct SolvableProcess {
        Process* process;     // non-owning
        int connectionCount;  // number of rates this process contributes
        int rateOffset;       // index of its first rate in the global rates vector
    };

    /**
     * Flat view of one solvable brick: the range of processes it owns.
     */
    struct SolvableBrickEntry {
        Brick* brick;      // non-owning
        int processStart;  // first index in _solvableProcesses
        int processEnd;    // one past the last index in _solvableProcesses
    };

    /**
     * Flat view of one process of a brick computed directly, with its slice of the
     * direct change-rate buffer.
     */
    struct DirectProcess {
        Process* process;           // non-owning
        WaterContainer* container;  // non-owning: the container the process draws from
        int outputFluxCount;        // number of output fluxes to link
        int connectionCount;        // number of rates the process produces
        int rateOffset;             // index of its first rate in _changeRatesNoSolver
    };

    /**
     * Flat view of one brick computed directly: the range of processes it owns.
     */
    struct DirectBrickEntry {
        Brick* brick;      // non-owning
        int processStart;  // first index in _directProcesses
        int processEnd;    // one past the last index in _directProcesses
    };

    /**
     * Flat view of one hydro unit for the direct pass: its splitters, then its bricks
     * computed directly, in declaration order.
     */
    struct DirectUnitEntry {
        int splitterStart;  // first index in _directSplitters
        int splitterEnd;    // one past the last index in _directSplitters
        int brickStart;     // first index in _directBricks
        int brickEnd;       // one past the last index in _directBricks
    };

  protected:
    std::unique_ptr<Solver> _solver;  // owning
    ModelHydro* _model;               // non-owning reference
    int _solvableConnectionCount;
    int _directConnectionCount;
    int _directRateCount;  // size of the direct change-rate buffer (fixed slices)
    vecDoublePt _stateVariableChanges;
    vector<Brick*> _iterableBricks;  // non-owning views into HydroUnits/SubBasin
    // Flattened traversal of the solvable bricks and processes, built once at
    // initialization: the model structure is fixed after the build, so the
    // brick -> process -> connection hierarchy needs no virtual walk on every solver
    // stage of every time step.
    vector<SolvableProcess> _solvableProcesses;
    vector<SolvableBrickEntry> _solvableBrickEntries;
    // Same flattening for the direct (pre-solver) pass over the hydro units.
    vector<Splitter*> _directSplitters;  // non-owning, grouped by hydro unit
    vector<DirectProcess> _directProcesses;
    vector<DirectBrickEntry> _directBricks;
    vector<DirectUnitEntry> _directUnits;
    axd _changeRatesNoSolver;
    axd _ratesBeforeSweep;  // scratch buffer for the constraint fixpoint iteration

  private:
    /**
     * Validate the flux topology of the two-phase time step: no solved brick may send
     * water to a brick computed directly, as direct bricks are processed before the solver.
     *
     * @throws ModelConfigError if a solved brick feeds a brick computed directly.
     */
    void ValidateFluxTopology() const;

    /**
     * Store the state variable changes.
     *
     * @param values the state variable changes to store.
     */
    void StoreStateVariableChanges(std::span<double*> values);

    /**
     * Flatten the solvable bricks and their processes into the traversal tables used by
     * the per-time-step routines (EvaluateRates, ConstrainRates, ApplyRates).
     */
    void BuildTraversalTables();

    /**
     * Apply direct changes to the brick.
     *
     * @param brickEntry the brick to apply changes to, with its process range.
     * @param timeStepInDays the time step in days.
     */
    void ApplyDirectChanges(const DirectBrickEntry& brickEntry, double timeStepInDays);
};

#endif  // HYDROBRICKS_PROCESSOR_H
