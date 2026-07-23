#include "Processor.h"

#include "FluxToBrick.h"
#include "ModelHydro.h"
#include "SubBasin.h"

Processor::Processor()
    : _solver(nullptr),
      _model(nullptr),
      _solvableConnectionCount(0),
      _directConnectionCount(0) {}

Processor::~Processor() = default;  // Automatic cleanup via unique_ptr

void Processor::Initialize(const SolverSettings& solverSettings) {
    // Should not be called twice
    if (_solver) {
        throw std::runtime_error("Processor::Initialize - Processor is already initialized.");
    }

    _solver = Solver::Factory(solverSettings);
    _solver->Connect(this);
    ConnectToElementsToSolve();
    ValidateFluxTopology();
    _solver->InitializeContainers();
    _changeRatesNoSolver = axd::Zero(_directConnectionCount);
}

void Processor::ValidateFluxTopology() const {
    // Direct bricks are processed before the solver runs, so their inputs must not come
    // from solver bricks: such water would only be picked up at the following time step,
    // silently breaking the surface (direct) -> ground (solver) phase ordering.
    for (auto brick : _iterableBricks) {
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);
            for (int j = 0; j < process->GetOutputFluxCount(); ++j) {
                auto fluxToBrick = dynamic_cast<FluxToBrick*>(process->GetOutputFlux(j));
                if (fluxToBrick == nullptr) {
                    continue;  // Fluxes to the outlet or the atmosphere are fine.
                }
                Brick* target = fluxToBrick->GetTargetBrick();
                if (target && !target->NeedsSolver()) {
                    throw ModelConfigError(
                        std::format("The brick '{}' (solved) sends water to the brick '{}' (computed directly). "
                                    "Bricks computed directly are processed before the solver and must not receive "
                                    "water from solved bricks.",
                                    brick->GetName(), target->GetName()));
                }
            }
        }
    }
}

void Processor::SetModel(ModelHydro* model) {
    _model = model;
}

void Processor::ConnectToElementsToSolve() {
    SubBasin* basin = _model->GetSubBasin();

    // Two-phase time step contract: bricks computed directly (surface components, land
    // covers) form the discrete phase, processed sequentially in declaration order before
    // the solver; bricks needing the solver (storages) form the continuous phase,
    // integrated together as one coupled system. The classification comes exclusively
    // from the brick property (NeedsSolver), never from the declaration position.
    // ValidateFluxTopology() enforces that no solver brick feeds a direct brick.
    int hydroUnitCount = basin->GetHydroUnitCount();
    for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        int brickCount = unit->GetBrickCount();
        for (int iBrick = 0; iBrick < brickCount; ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);

            if (brick->NeedsSolver()) {
                _iterableBricks.push_back(brick);

                // Get state variables from bricks
                vecDoublePt bricksValues = brick->GetDynamicContentChanges();
                StoreStateVariableChanges(bricksValues);

                // Get state variables from processes
                vecDoublePt processValues = brick->GetStateVariableChangesFromProcesses();
                StoreStateVariableChanges(processValues);

                // Count connections
                _solvableConnectionCount += brick->GetProcessConnectionCount();
            } else {
                // Count connections
                _directConnectionCount += brick->GetProcessConnectionCount();
            }
        }
    }

    int basinBrickCount = basin->GetBrickCount();
    for (int iBrick = 0; iBrick < basinBrickCount; ++iBrick) {
        Brick* brick = basin->GetBrick(iBrick);

        // Add the bricks need a solver here
        _iterableBricks.push_back(brick);

        // Get state variables from bricks
        vecDoublePt bricksValues = brick->GetDynamicContentChanges();
        StoreStateVariableChanges(bricksValues);

        // Get state variables from processes
        vecDoublePt processValues = brick->GetStateVariableChangesFromProcesses();
        StoreStateVariableChanges(processValues);

        // Count connections
        _solvableConnectionCount += brick->GetProcessConnectionCount();
    }
}

void Processor::StoreStateVariableChanges(std::span<double*> values) {
    if (!values.empty()) {
        for (auto const& value : values) {
            _stateVariableChanges.push_back(value);
        }
    }
}

int Processor::GetStateVariableCount() const {
    return static_cast<int>(_stateVariableChanges.size());
}

void Processor::GatherState(axd& state) const {
    assert(state.size() == static_cast<int>(_stateVariableChanges.size()));
    for (auto [i, value] : std::views::enumerate(_stateVariableChanges)) {
        state(i) = *value;
    }
}

void Processor::ScatterState(const axd& state) {
    assert(state.size() == static_cast<int>(_stateVariableChanges.size()));
    for (auto [i, value] : std::views::enumerate(_stateVariableChanges)) {
        *value = state(i);
    }
}

void Processor::ResetState() {
    for (auto value : _stateVariableChanges) {
        *value = 0;
    }
}

void Processor::EvaluateRates(axd& rates, double timeStepInDays, bool applyConstraints) {
    int iRate = 0;
    for (auto brick : _iterableBricks) {
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);

            // Get the change rates (per day) independently of the time step and constraints (null bricks handled).
            // Reference into the process's reusable buffer; consumed below before the next process is queried.
            const vecDouble& processRates = process->GetChangeRates();

            for (int j = 0; j < processRates.size(); ++j) {
                assert(rates.size() > iRate);
                rates(iRate) = processRates[j];

                // Link to fluxes to enforce subsequent constraints
                if (applyConstraints) {
                    process->StoreInOutgoingFlux(&rates(iRate), j);
                }
                iRate++;
            }
        }
    }

    if (applyConstraints) {
        EnforceConstraints(rates, timeStepInDays);
    }
}

void Processor::ConstrainRates(axd& rates, double timeStepInDays) {
    int iRate = 0;
    for (auto brick : _iterableBricks) {
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);
            for (int j = 0; j < process->GetConnectionCount(); ++j) {
                assert(rates.size() > iRate);
                // Link to fluxes to enforce subsequent constraints
                process->StoreInOutgoingFlux(&rates(iRate), j);
                iRate++;
            }
        }
    }

    EnforceConstraints(rates, timeStepInDays);
}

void Processor::EnforceConstraints(axd& rates, double timeStepInDays) {
    // The brick constraints (e.g. maximum capacity or avoid negative values) mutate the
    // rates through the linked flux pointers. A clamp on one brick changes what an
    // already-swept brick sees (the rates are shared between the source and target
    // bricks), so a single sweep would depend on the brick iteration order; iterate the
    // sweep until the rates are stable instead. Convergence is typically reached after
    // two sweeps (the second one confirming the first).
    constexpr int maxSweeps = 10;
    for (int sweep = 0; sweep < maxSweeps; ++sweep) {
        _ratesBeforeSweep = rates;
        for (auto brick : _iterableBricks) {
            brick->ApplyConstraints(timeStepInDays);
        }
        if (((rates - _ratesBeforeSweep).abs() <= PRECISION).all()) {
            return;
        }
    }
    LogWarning("The storage constraints did not stabilize after {} sweeps.", maxSweeps);
}

void Processor::ApplyRates(const axd& rates, double timeStepInDays) {
    int iRate = 0;
    for (auto brick : _iterableBricks) {
        if (brick->IsNull()) {
            continue;
        }
        brick->UpdateContentFromInputs();
        for (int i = 0; i < brick->GetProcessCount(); ++i) {
            auto process = brick->GetProcess(i);
            for (int iConnect = 0; iConnect < process->GetConnectionCount(); ++iConnect) {
                process->ApplyChange(iConnect, rates(iRate), timeStepInDays);
                iRate++;
            }
        }
    }
}

void Processor::FinalizeTimeStep() {
    for (auto brick : _iterableBricks) {
        if (brick->IsNull()) {
            continue;
        }
        brick->Finalize();
    }
}

bool Processor::ProcessTimeStep(double timeStepInDays) {
    assert(_model);

    SubBasin* basin = _model->GetSubBasin();

    // Process the bricks that do not need a solver.
    int ptIndex = 0;
    int hydroUnitCount = basin->GetHydroUnitCount();
    for (int iUnit = 0; iUnit < hydroUnitCount; ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        int splitterCount = unit->GetSplitterCount();
        for (int iSplitter = 0; iSplitter < splitterCount; ++iSplitter) {
            Splitter* splitter = unit->GetSplitter(iSplitter);
            splitter->Compute();
        }
        int brickCount = unit->GetBrickCount();
        for (int iBrick = 0; iBrick < brickCount; ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);
            if (brick->NeedsSolver()) {
                continue;
            }
            if (brick->IsNull()) {
                continue;
            }

            ApplyDirectChanges(brick, ptIndex, timeStepInDays);
        }
    }

    // Process the bricks that need a solver
    if (!_solver->Solve(timeStepInDays)) {
        return false;
    }

    if (!basin->ComputeOutletDischarge()) {
        return false;
    }

    return true;
}

void Processor::ApplyDirectChanges(Brick* brick, int& ptIndex, double timeStepInDays) {
    brick->UpdateContentFromInputs();

    // Initialize the change rates to 0 and link to fluxes
    int iRate = ptIndex;
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        auto process = brick->GetProcess(i);
        for (int j = 0; j < process->GetOutputFluxCount(); ++j) {
            assert(_changeRatesNoSolver.rows() > iRate);
            _changeRatesNoSolver(iRate) = 0;

            // Link to fluxes to enforce subsequent constraints
            process->StoreInOutgoingFlux(&_changeRatesNoSolver(iRate), j);
            iRate++;
        }
    }

    iRate = ptIndex;
    for (int i = 0; i < brick->GetProcessCount(); ++i) {
        auto process = brick->GetProcess(i);

        // Get the change rates (per day) independently of the time step and constraints.
        // Reference into the process's reusable buffer; consumed within this iteration.
        const vecDouble& rates = process->GetChangeRates();

        int iRateCopy = iRate;
        for (double rate : rates) {
            assert(_changeRatesNoSolver.rows() > iRateCopy);
            _changeRatesNoSolver(iRateCopy) = rate;
            iRateCopy++;
        }

        // Apply constraints for the current brick (e.g. maximum capacity or avoid negative values)
        process->GetWaterContainer()->ApplyConstraints(timeStepInDays);

        // Apply changes
        for (int j = 0; j < rates.size(); ++j) {
            process->ApplyChange(j, _changeRatesNoSolver(iRate), timeStepInDays);
            _changeRatesNoSolver(iRate) = 0;
            iRate++;
            ptIndex++;
        }
    }

    brick->Finalize();
}
