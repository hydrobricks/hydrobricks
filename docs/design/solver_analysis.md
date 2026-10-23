# Solver design review

Assessment of the hydrobricks ODE solver architecture (`core/src/base/Solver*`,
`Processor`, `WaterContainer::ApplyConstraints`) and recommendations for making it
more rigorous.

## Overall verdict

The design is fundamentally sound, and the philosophy of "surface = instantaneous,
ground = solved" is already ~80% implemented — it just hasn't been named or pushed to
its logical conclusion. The messiness is concentrated in a few specific places, not the
whole design.

---

## 1. "Compute rates once then finalize — best way?"

For **explicit Euler**, yes — one evaluation is correct. The problem isn't the number of
evaluations; it's that the constraint projection makes the higher-order solvers (Heun,
RK4) mostly **decorative**.

In `SolverHeunExplicit` / `SolverRK4`:
- Only the first `ComputeChangeRates(0)` runs with `applyConstraints=true`.
- Intermediate stages (`ComputeChangeRates(1, false)`, etc.) are computed on states
  advanced by *unconstrained* combinations; only the final combined rate is clamped via
  `ApplyConstraintsFor`.

Deeper issue: `f(S)` is **non-smooth**. Every `min/max` clamp in
`WaterContainer::ApplyConstraints`, every threshold process, and every
`GetChangeRates()` early return of zero at empty storage is a discontinuity in the RHS.
RK4's 4th-order accuracy assumes `f ∈ C⁴`. Near any such event (most timesteps for at
least one store), RK4 silently degrades to **first order** — 4× the RHS evaluations for
accuracy you rarely get.

**Takeaway:** don't invest more in RK4. Real accuracy/stability gains come from analytic
linear-reservoir solutions and adaptive sub-stepping (§7).

## 2. A more rigorous way to handle constraints

Constraints are currently a **single-pass, brick-order-dependent projection** of the
rate vector. That is the root of the "backward fluxes leak / fluxes must flow toward
later-declared bricks" rule — a *symptom* of a non-rigorous solve: `ApplyConstraints`
clamps brick-by-brick in iteration order, so an upstream brick's clamp isn't seen by a
downstream brick already processed.

Two levels of rigor:

- **Cheap/pragmatic:** iterate the projection to a fixpoint (repeat the brick sweep
  until no rate changes by more than ε). Removes the ordering dependency without new
  math; usually converges in 2–3 sweeps for a DAG.
- **Rigorous:** the non-negativity + capacity constraints make this a **Linear
  Complementarity Problem** per timestep (rates ≥ 0, content ≥ 0, complementarity
  between "store empty" and "outflow reduced"). Solvable exactly, order-independent;
  would let the flux-ordering rule be deleted entirely.

Start with the fixpoint iteration — 90% of the benefit, a fraction of the work.

## 3. Not delaying water transit

The one-timestep-per-brick lag in a serial cascade is inherent to **explicit** stepping.
Three real fixes, increasing value:

1. **Instantaneous fluxes** (`FluxToBrickInstantaneous`) — already exists, exactly the
   right primitive, moves water within the step. Underused.
2. **Analytic linear-reservoir integration.** For `outflow:linear` (dS/dt = I − kS) with
   constant step inflow I, the exact solution is:
   ```
   S(t+h) = S·e^(−kh) + (I/k)(1 − e^(−kh))
   outflow_volume = I·h − (S(t+h) − S)
   ```
   Exact, unconditionally stable, timestep-insensitive, no artificial delay beyond the
   physical residence time 1/k. Highest-value change for a model whose ground stores are
   mostly linear reservoirs (HBV/PREVAH-family rely on exactly this).
3. **Implicit (backward Euler / Crank–Nicolson)** for the coupled ground system —
   unconditionally stable, exact within-step transfer for linear cascades. More
   machinery (needs a linear/Newton solve), but the general answer for nonlinear stores.

## 4 & 5. Surface = instantaneous, ground = solved

Strongly agree, and it's most of the way there:
- `LandCover`/`SurfaceComponent` (→ `Snowpack`, `Glacier`, `InterceptionStorage`) set
  `_needsSolver = false` and run once in `Processor::ProcessTimeStep` *before* the solver.
- `Storage` (ground) keeps `_needsSolver = true`.

What's **not** rigorous is the boundary rule in `Processor::ConnectToElementsToSolve`:

```cpp
if (brick->NeedsSolver() || solverRequired) { ... solverRequired = true; }
```

Once *any* brick in a unit needs the solver, **every subsequently-declared brick is
dragged into the solver too**, regardless of surface/ground intent. "Surface is direct"
holds only if all surface bricks are declared before all ground bricks — a fragile,
implicit ordering contract.

To make the philosophy rigorous:
- Classify direct-vs-solved by an **explicit brick property** (`computedDirectly` →
  `SetNeedsSolver(false)` already exists), not declaration position.
- Make the surface layer a proper **topologically-ordered forward-substitution pass**
  with instantaneous fluxes, so surface water cascades fully to the ground stores within
  the step. The solver then only ever sees the stiff/nonlinear ground subsystem.

This cleanly separates transport (surface, resolved by ordering) from storage dynamics
(ground, resolved by the ODE integrator).

## 6. Making the solver less messy

Core structural problem: `Solver` is **not** an ODE integrator. It knows about bricks,
processes, fluxes, containers, constraint clamping, and flux-pointer linking. A rigorous
integrator should see only a state vector `y`, a function `f(t, y) → dy/dt`, and a
projection `P(y)`. Concretely:

- **Extract an RHS evaluator.** `ComputeChangeRates` (inline constraints) and
  `ApplyConstraintsFor` (re-links fluxes + constraints) are two half-duplicated versions
  of the same thing. Collapse into one.
- **Integrators become tiny.** Euler/Heun/RK4 should each be ~5 lines over `y` and `f`,
  with an explicit Butcher tableau — no scattered `_changeRates.col(4)` magic indices, no
  `_nIterations = 5` (4 stages + 1 combined) to reverse-engineer.
- **Kill the `SetStateVariablesToAvgOf` delta-reset dance.** With an explicit state
  vector, intermediate evaluations become `y_stage = y0 + h·(a21·k1)` directly.
- **One constraint code path**, applied consistently at every stage (or deliberately
  not — but decide and document).

No behavior change; makes the higher-order solvers correct by construction and the code
auditable.

## 7. Additional solvers worth having

Ranked by real value for this model:

1. **Analytic linear reservoir** (§3.2) — per-process exact solution. Biggest bang.
2. **Adaptive embedded RK (RK23 Bogacki–Shampine or RK45 Dormand–Prince)** with
   error-controlled sub-stepping inside the hydrological timestep. Real stability/accuracy
   on stiff ground stores without a tiny global step — far more useful than fixed RK4.
3. **Fixed sub-stepping Euler** — trivial, cheap stability win, good baseline.
4. **Backward Euler / Crank–Nicolson** (§3.3) — unconditional stability generically.

Keep RK4 only as a reference/validation solver, and document that it does not deliver
4th-order accuracy across constraint events.

---

## Recommended order of work

1. **Refactor `Solver` into (state vector + `f` + projection) with tiny integrators.**
   Pure cleanup, no behavior change, unlocks everything else.
2. **Fixpoint constraint iteration** to kill the brick-ordering dependency.
3. **Analytic linear-reservoir integration** for `outflow:linear` — biggest hydrological
   win; removes timestep sensitivity and transit delay for linear stores.
4. **Explicit surface/ground classification** + topological forward-substitution surface
   pass, so the solver only touches ground stores.
5. **Adaptive embedded RK** for the ground subsystem.

Each is independent and separately testable against the C++ gtest suite (the real
regression guard — pytest alone won't catch solver regressions).
